/*
 * The GDB/MI backend: runs `gdb --interpreter=mi3` on a pair of pipes,
 * sends numbered commands and turns result and async records into
 * dbg_events (DESIGN.md 7.2).
 *
 * GDB runs in all-stop, synchronous mode: commands written while the
 * program runs wait in the pipe until it stops, which is what dbxl wants.
 * The program gets dbxl's terminal (or /dev/null), never the MI pipes.
 * The terminal is attached by redirections for the shell GDB starts the
 * program with, not by -inferior-tty-set: that makes GDB try to make the
 * terminal the program's controlling terminal, which fails for a terminal
 * that already controls dbxl's session, and GDB then prints a warning (as
 * a raw MI record) on it.
 */
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "backend/backend.h"
#include "backend/gdbmi/mi.h"
#include "dbxl_py.h"                 /* generated from dbxl.py */

#define MAX_PENDING 256
#define MAX_FRAMES 256

struct pending {
    long token;
    enum dbg_request req;
    void *cookie;
    int id;                        /* bp_delete: the breakpoint */
};

struct gdbmi {
    dbg_backend base;
    pid_t pid;
    int to_gdb, from_gdb;
    char *buf;
    size_t len, cap;
    long next_token;
    struct pending pending[MAX_PENDING];
    int npending;
    char run_to[256];
    dbg_frame frames[MAX_FRAMES];
    FILE *log;                     /* $DBXL_MI_LOG: the MI traffic */
    int inferior_pid;
    char helper_dir[64];           /* holds dbxl.py while GDB runs */
    /* result buffers, valid during the event callback */
    dbg_insn *insns;
    dbg_register *regs;
    dbg_symbol *syms;
    dbg_thread *threads;
    unsigned char *bytes;
    char **names;
    int nnames;
};

static void emit(struct gdbmi *g, dbg_event *ev)
{
    g->base.fn(ev, g->base.arg);
}

static void sendf(struct gdbmi *g, enum dbg_request req, void *cookie, int id,
                  const char *fmt, ...)
{
    char cmd[4096];
    int n;
    va_list ap;
    long token = ++g->next_token;

    n = snprintf(cmd, sizeof cmd, "%ld", token);
    va_start(ap, fmt);
    n += vsnprintf(cmd + n, sizeof cmd - (size_t)n - 1, fmt, ap);
    va_end(ap);
    if (n > (int)sizeof cmd - 2)
        n = (int)sizeof cmd - 2;
    cmd[n++] = '\n';
    if (g->log) {
        fprintf(g->log, "> %.*s", n, cmd);
        fflush(g->log);
    }
    if (g->npending < MAX_PENDING) {
        struct pending *p = &g->pending[g->npending++];
        p->token = token;
        p->req = req;
        p->cookie = cookie;
        p->id = id;
    }
    if (g->to_gdb >= 0) {
        const char *s = cmd;
        while (n > 0) {
            ssize_t w = write(g->to_gdb, s, (size_t)n);
            if (w < 0) {
                if (errno == EINTR)
                    continue;
                break;
            }
            s += w;
            n -= (int)w;
        }
    }
}

static bool take_pending(struct gdbmi *g, long token, struct pending *out)
{
    for (int i = 0; i < g->npending; i++)
        if (g->pending[i].token == token) {
            *out = g->pending[i];
            memmove(&g->pending[i], &g->pending[i + 1],
                    (size_t)(g->npending - i - 1) * sizeof g->pending[0]);
            g->npending--;
            return true;
        }
    return false;
}

static void copy(char *dst, size_t size, const char *src)
{
    snprintf(dst, size, "%s", src ? src : "");
}

static void parse_frame(const mi_value *t, dbg_frame *f)
{
    const char *addr = mi_str(t, "addr");

    memset(f, 0, sizeof *f);
    f->level = (int)mi_int(t, "level", 0);
    f->addr = addr ? strtoull(addr, NULL, 0) : 0;
    copy(f->func, sizeof f->func, mi_str(t, "func"));
    copy(f->file, sizeof f->file, mi_str(t, "file"));
    copy(f->fullname, sizeof f->fullname, mi_str(t, "fullname"));
    copy(f->from, sizeof f->from, mi_str(t, "from"));
    f->line = (int)mi_int(t, "line", 0);
}

static void parse_bkpt(const mi_value *b, dbg_breakpoint *bp)
{
    const mi_value *locs = mi_get(b, "locations");
    const mi_value *where = b;
    const char *en = mi_str(b, "enabled");

    memset(bp, 0, sizeof *bp);
    bp->id = (int)mi_int(b, "number", 0);
    bp->enabled = !en || strcmp(en, "y") == 0;
    bp->nlocations = 1;
    if (locs && locs->kind == MI_LIST && locs->child) {
        bp->nlocations = 0;
        for (const mi_value *l = locs->child; l; l = l->next)
            bp->nlocations++;
        if (!mi_str(b, "line"))
            where = locs->child;
    }
    copy(bp->func, sizeof bp->func, mi_str(where, "func"));
    copy(bp->file, sizeof bp->file, mi_str(where, "file"));
    copy(bp->fullname, sizeof bp->fullname, mi_str(where, "fullname"));
    bp->line = (int)mi_int(where, "line", 0);
    {
        const char *a = mi_str(where, "addr");
        bp->addr = a && a[0] == '0' ? strtoull(a, NULL, 16) : 0;
    }
}

static char *dup_str(const mi_value *t, const char *key)
{
    const char *s = mi_str(t, key);

    return strdup(s ? s : "");
}

static enum dbg_kind parse_kind(const char *k)
{
    static const struct { const char *name; enum dbg_kind kind; } kinds[] = {
        { "int", DBG_K_INT }, { "uint", DBG_K_UINT }, { "char", DBG_K_CHAR },
        { "uchar", DBG_K_UCHAR }, { "bool", DBG_K_BOOL },
        { "float", DBG_K_FLOAT }, { "enum", DBG_K_ENUM },
        { "pointer", DBG_K_POINTER }, { "array", DBG_K_ARRAY },
        { "struct", DBG_K_STRUCT }, { "union", DBG_K_UNION },
        { "func", DBG_K_FUNC }, { "error", DBG_K_ERROR },
    };

    for (size_t i = 0; k && i < sizeof kinds / sizeof kinds[0]; i++)
        if (strcmp(k, kinds[i].name) == 0)
            return kinds[i].kind;
    return DBG_K_OTHER;
}

static void parse_value(const mi_value *t, dbg_value *v)
{
    const mi_value *ch = mi_get(t, "ch"), *p = mi_get(t, "p");
    const char *a = mi_str(t, "a"), *b = mi_str(t, "b"), *s = mi_str(t, "s");

    memset(v, 0, sizeof *v);
    v->name = dup_str(t, "n");
    v->kind = parse_kind(mi_str(t, "k"));
    v->type_name = dup_str(t, "t");
    v->type_style = dup_str(t, "ts");
    v->tag = dup_str(t, "tag");
    v->size = (int)mi_int(t, "sz", 0);
    v->has_addr = a && *a;
    v->addr = v->has_addr ? strtoull(a, NULL, 16) : 0;
    v->value = dup_str(t, "v");
    v->bits = b && *b ? strtoull(b, NULL, 16) : 0;
    v->str = s ? strdup(s) : NULL;
    v->expr = dup_str(t, "e");
    v->lo = mi_int(t, "lo", 0);
    if (ch && ch->kind == MI_LIST) {
        int n = 0;
        for (const mi_value *c = ch->child; c; c = c->next)
            n++;
        v->children = calloc((size_t)(n ? n : 1), sizeof *v->children);
        for (const mi_value *c = ch->child; c; c = c->next)
            parse_value(c, &v->children[v->nchildren++]);
    }
    if (p && p->kind == MI_TUPLE) {
        v->pointee = calloc(1, sizeof *v->pointee);
        parse_value(p, v->pointee);
    }
}

static void parse_group(const mi_value *list, dbg_value_group *grp)
{
    int n = 0;

    for (const mi_value *c = list ? list->child : NULL; c; c = c->next)
        n++;
    grp->vars = calloc((size_t)(n ? n : 1), sizeof *grp->vars);
    for (const mi_value *c = list ? list->child : NULL; c; c = c->next)
        parse_value(c, &grp->vars[grp->nvars++]);
}

static dbg_values *parse_values(const mi_value *r, enum dbg_scope scope,
                                int level)
{
    dbg_values *vals = calloc(1, sizeof *vals);

    vals->scope = scope;
    vals->level = level;
    vals->ptrsize = (int)mi_int(r, "ptrsize", 8);
    if (scope == DBG_SCOPE_LOCALS) {
        vals->groups = calloc(1, sizeof *vals->groups);
        vals->ngroups = 1;
        parse_group(mi_get(r, "vars"), &vals->groups[0]);
    } else {
        const mi_value *files = mi_get(r, "files");
        int n = 0;

        for (const mi_value *f = files ? files->child : NULL; f; f = f->next)
            n++;
        vals->groups = calloc((size_t)(n ? n : 1), sizeof *vals->groups);
        for (const mi_value *f = files ? files->child : NULL; f; f = f->next) {
            dbg_value_group *grp = &vals->groups[vals->ngroups++];
            grp->file = dup_str(f, "file");
            grp->fullname = dup_str(f, "fullname");
            parse_group(mi_get(f, "vars"), grp);
        }
    }
    return vals;
}

static void free_value(dbg_value *v)
{
    free(v->name);
    free(v->type_name);
    free(v->type_style);
    free(v->tag);
    free(v->value);
    free(v->str);
    free(v->expr);
    for (int i = 0; i < v->nchildren; i++)
        free_value(&v->children[i]);
    free(v->children);
    if (v->pointee) {
        free_value(v->pointee);
        free(v->pointee);
    }
}

void dbg_values_free(dbg_values *vals)
{
    if (!vals)
        return;
    for (int i = 0; i < vals->ngroups; i++) {
        dbg_value_group *grp = &vals->groups[i];
        for (int k = 0; k < grp->nvars; k++)
            free_value(&grp->vars[k]);
        free(grp->vars);
        free(grp->file);
        free(grp->fullname);
    }
    free(vals->groups);
    free(vals);
}

static void on_result(struct gdbmi *g, const mi_record *r)
{
    struct pending p;
    dbg_event ev;

    if (r->token < 0 || !take_pending(g, r->token, &p))
        return;
    memset(&ev, 0, sizeof ev);
    ev.request = p.req;
    ev.cookie = p.cookie;
    if (strcmp(r->klass, "error") == 0) {
        ev.type = DBG_EV_ERROR;
        ev.message = mi_str(r->results, "msg");
        if (!ev.message)
            ev.message = "";
        emit(g, &ev);
        return;
    }
    switch (p.req) {
    case DBG_REQ_BP_LINE:
    case DBG_REQ_BP_FUNC: {
        const mi_value *b = mi_get(r->results, "bkpt");
        if (!b)
            return;
        ev.type = DBG_EV_BP_SET;
        parse_bkpt(b, &ev.bp);
        emit(g, &ev);
        break;
    }
    case DBG_REQ_VALUES:
        ev.type = DBG_EV_VALUES;
        /* id carries scope * 1000 + level. */
        ev.values = parse_values(r->results, (enum dbg_scope)(p.id / 1000),
                                 p.id % 1000);
        emit(g, &ev);
        break;
    case DBG_REQ_ASSIGN:
        ev.type = DBG_EV_ASSIGNED;
        emit(g, &ev);
        break;
    case DBG_REQ_BP_ADDR:
    case DBG_REQ_BP_ENABLE:
    case DBG_REQ_BP_CONDITION: {
        const mi_value *b = mi_get(r->results, "bkpt");
        if (p.req == DBG_REQ_BP_ADDR) {
            if (!b)
                return;
            ev.type = DBG_EV_BP_SET;
            parse_bkpt(b, &ev.bp);
        } else {
            /* -break-enable/-condition answer ^done; report the id. */
            ev.type = DBG_EV_ASSIGNED;
            ev.bp.id = p.id;
        }
        emit(g, &ev);
        break;
    }
    case DBG_REQ_WRITE_MEMORY:
        ev.type = DBG_EV_ASSIGNED;
        emit(g, &ev);
        break;
    case DBG_REQ_DISASM: {
        const mi_value *list = mi_get(r->results, "asm_insns");
        int n = 0;

        for (const mi_value *c = list ? list->child : NULL; c; c = c->next)
            n++;
        free(g->insns);
        g->insns = calloc((size_t)(n ? n : 1), sizeof *g->insns);
        n = 0;
        for (const mi_value *c = list ? list->child : NULL; c; c = c->next) {
            dbg_insn *in = &g->insns[n++];
            const char *a = mi_str(c, "address");
            in->addr = a ? strtoull(a, NULL, 16) : 0;
            in->offset = mi_int(c, "offset", 0);
            copy(in->func, sizeof in->func, mi_str(c, "func-name"));
            copy(in->text, sizeof in->text, mi_str(c, "inst"));
        }
        ev.type = DBG_EV_DISASM;
        ev.insns = g->insns;
        ev.ninsns = n;
        emit(g, &ev);
        break;
    }
    case DBG_REQ_REGISTERS: {
        const mi_value *list = mi_get(r->results, "regs");
        int n = 0;

        for (const mi_value *c = list ? list->child : NULL; c; c = c->next)
            n++;
        free(g->regs);
        g->regs = calloc((size_t)(n ? n : 1), sizeof *g->regs);
        n = 0;
        for (const mi_value *c = list ? list->child : NULL; c; c = c->next) {
            dbg_register *rg = &g->regs[n++];
            const char *v = mi_str(c, "v");
            copy(rg->name, sizeof rg->name, mi_str(c, "n"));
            rg->value = v && *v ? strtoull(v, NULL, 16) : 0;
            rg->size = v && *v ? (int)mi_int(c, "sz", 0) : 0;
        }
        ev.type = DBG_EV_REGISTERS;
        ev.regs = g->regs;
        ev.nregs = n;
        emit(g, &ev);
        break;
    }
    case DBG_REQ_SYMBOLS: {
        const mi_value *files = mi_get(r->results, "files");
        const mi_value *funcs = mi_get(r->results, "functions");
        int nf = 0, nx = 0, k = 0;

        for (const mi_value *c = files ? files->child : NULL; c; c = c->next)
            nf++;
        for (const mi_value *c = funcs ? funcs->child : NULL; c; c = c->next)
            nx++;
        free(g->syms);
        g->syms = calloc((size_t)(nf + nx + 1), sizeof *g->syms);
        for (const mi_value *c = files ? files->child : NULL; c; c = c->next) {
            dbg_symbol *sy = &g->syms[k++];
            copy(sy->file, sizeof sy->file, mi_str(c, "file"));
            copy(sy->fullname, sizeof sy->fullname, mi_str(c, "fullname"));
        }
        for (const mi_value *c = funcs ? funcs->child : NULL; c; c = c->next) {
            dbg_symbol *sy = &g->syms[k++];
            const char *a = mi_str(c, "addr");
            copy(sy->name, sizeof sy->name, mi_str(c, "name"));
            copy(sy->file, sizeof sy->file, mi_str(c, "file"));
            copy(sy->fullname, sizeof sy->fullname, mi_str(c, "fullname"));
            sy->line = (int)mi_int(c, "line", 0);
            sy->addr = a ? strtoull(a, NULL, 16) : 0;
        }
        ev.type = DBG_EV_SYMBOLS;
        ev.files = g->syms;
        ev.nfiles = nf;
        ev.funcs = g->syms + nf;
        ev.nfuncs = nx;
        emit(g, &ev);
        break;
    }
    case DBG_REQ_DATA_START: {
        const char *a = mi_str(r->results, "addr");
        ev.type = DBG_EV_DATA_START;
        ev.addr = a ? strtoull(a, NULL, 16) : 0;
        emit(g, &ev);
        break;
    }
    case DBG_REQ_MEMORY: {
        const mi_value *list = mi_get(r->results, "memory");
        const mi_value *m = list ? list->child : NULL;
        const char *hex = m ? mi_str(m, "contents") : NULL;
        const char *begin = m ? mi_str(m, "begin") : NULL;
        int n = hex ? (int)strlen(hex) / 2 : 0;

        free(g->bytes);
        g->bytes = malloc((size_t)(n ? n : 1));
        for (int i = 0; i < n; i++) {
            char h[3] = { hex[2 * i], hex[2 * i + 1], 0 };
            g->bytes[i] = (unsigned char)strtoul(h, NULL, 16);
        }
        ev.type = DBG_EV_MEMORY;
        ev.addr = begin ? strtoull(begin, NULL, 16) : 0;
        ev.bytes = g->bytes;
        ev.nbytes = n;
        emit(g, &ev);
        break;
    }
    case DBG_REQ_THREADS: {
        const mi_value *list = mi_get(r->results, "threads");
        const char *cur = mi_str(r->results, "current-thread-id");
        int n = 0;

        for (const mi_value *c = list ? list->child : NULL; c; c = c->next)
            n++;
        free(g->threads);
        g->threads = calloc((size_t)(n ? n : 1), sizeof *g->threads);
        n = 0;
        for (const mi_value *c = list ? list->child : NULL; c; c = c->next) {
            dbg_thread *t = &g->threads[n++];
            const char *tid = mi_str(c, "target-id"), *p2;
            const char *id = mi_str(c, "id");
            t->id = id ? atoi(id) : 0;
            t->current = id && cur && strcmp(id, cur) == 0;
            if (tid && ((p2 = strstr(tid, "LWP ")) || (p2 = strstr(tid, "process "))))
                t->lwp = strtol(strchr(p2, ' ') + 1, NULL, 10);
            copy(t->state, sizeof t->state, mi_str(c, "state"));
            copy(t->func, sizeof t->func, mi_str(mi_get(c, "frame"), "func"));
        }
        ev.type = DBG_EV_THREADS;
        ev.threads = g->threads;
        ev.nthreads = n;
        emit(g, &ev);
        break;
    }
    case DBG_REQ_TYPES: {
        const mi_value *list = mi_get(r->results, "types");

        for (int i = 0; i < g->nnames; i++)
            free(g->names[i]);
        free(g->names);
        g->nnames = 0;
        for (const mi_value *c = list ? list->child : NULL; c; c = c->next)
            g->nnames++;
        g->names = calloc((size_t)g->nnames + 1, sizeof *g->names);
        g->nnames = 0;
        for (const mi_value *c = list ? list->child : NULL; c; c = c->next)
            g->names[g->nnames++] = strdup(c->str ? c->str : "");
        ev.type = DBG_EV_TYPES;
        ev.names = (const char *const *)g->names;
        ev.nnames = g->nnames;
        emit(g, &ev);
        break;
    }
    case DBG_REQ_BP_DELETE:
        ev.type = DBG_EV_BP_DELETED;
        ev.bp.id = p.id;
        emit(g, &ev);
        break;
    case DBG_REQ_FRAMES: {
        const mi_value *stack = mi_get(r->results, "stack");
        int n = 0;

        if (stack)
            for (const mi_value *f = stack->child; f && n < MAX_FRAMES; f = f->next)
                parse_frame(f, &g->frames[n++]);
        ev.type = DBG_EV_FRAMES;
        ev.frames = g->frames;
        ev.nframes = n;
        emit(g, &ev);
        break;
    }
    default:
        break;
    }
}

static void on_stopped(struct gdbmi *g, const mi_record *r)
{
    const char *reason = mi_str(r->results, "reason");
    const mi_value *frame = mi_get(r->results, "frame");
    dbg_event ev;

    memset(&ev, 0, sizeof ev);
    ev.type = DBG_EV_STOPPED;
    ev.reason = DBG_STOP_OTHER;
    if (frame)
        parse_frame(frame, &ev.frame);
    if (!reason) {
        /* e.g. the stop after an interrupted exec */
    } else if (strcmp(reason, "breakpoint-hit") == 0) {
        const char *disp = mi_str(r->results, "disp");
        ev.reason = disp && strcmp(disp, "del") == 0 ? DBG_STOP_RUN_TO
                                                     : DBG_STOP_BREAKPOINT;
        ev.bkptno = (int)mi_int(r->results, "bkptno", 0);
    } else if (strcmp(reason, "end-stepping-range") == 0 ||
               strcmp(reason, "function-finished") == 0 ||
               strcmp(reason, "location-reached") == 0) {
        ev.reason = DBG_STOP_STEP;
    } else if (strcmp(reason, "signal-received") == 0) {
        ev.reason = DBG_STOP_SIGNAL;
        ev.signame = mi_str(r->results, "signal-name");
    } else if (strcmp(reason, "exited-normally") == 0) {
        ev.type = DBG_EV_EXITED;
    } else if (strcmp(reason, "exited") == 0) {
        ev.type = DBG_EV_EXITED;
        /* GDB prints the code in octal ("012"). */
        ev.exit_code = (int)strtol(mi_str(r->results, "exit-code") ?
                                   mi_str(r->results, "exit-code") : "0",
                                   NULL, 8);
    } else if (strcmp(reason, "exited-signalled") == 0) {
        ev.type = DBG_EV_SIGNALLED;
        ev.signame = mi_str(r->results, "signal-name");
    }
    emit(g, &ev);
}

static void on_line(struct gdbmi *g, const char *line)
{
    mi_record *r = mi_parse(line);
    dbg_event ev;

    if (g->log) {
        fprintf(g->log, "< %s\n", line);
        fflush(g->log);
    }
    if (!r)
        return;
    switch (r->type) {
    case MI_RESULT:
        on_result(g, r);
        break;
    case MI_EXEC:
        if (strcmp(r->klass, "running") == 0) {
            memset(&ev, 0, sizeof ev);
            ev.type = DBG_EV_RUNNING;
            emit(g, &ev);
        } else if (strcmp(r->klass, "stopped") == 0) {
            on_stopped(g, r);
        }
        break;
    case MI_NOTIFY:
        if (strcmp(r->klass, "thread-group-started") == 0)
            g->inferior_pid = (int)mi_int(r->results, "pid", 0);
        else if (strcmp(r->klass, "thread-group-exited") == 0)
            g->inferior_pid = 0;
        break;
    default:
        break;
    }
    mi_record_free(r);
}

static void died(struct gdbmi *g)
{
    dbg_event ev;

    if (g->from_gdb >= 0)
        close(g->from_gdb);
    if (g->to_gdb >= 0)
        close(g->to_gdb);
    g->from_gdb = g->to_gdb = -1;
    if (g->pid > 0)
        waitpid(g->pid, NULL, WNOHANG);
    memset(&ev, 0, sizeof ev);
    ev.type = DBG_EV_DIED;
    emit(g, &ev);
}

static void gdbmi_readable(dbg_backend *b)
{
    struct gdbmi *g = (struct gdbmi *)b;
    char chunk[8192];
    ssize_t n;
    size_t start = 0;

    if (g->from_gdb < 0)
        return;
    n = read(g->from_gdb, chunk, sizeof chunk);
    if (n < 0 && (errno == EINTR || errno == EAGAIN))
        return;
    if (n <= 0) {
        died(g);
        return;
    }
    if (g->len + (size_t)n + 1 > g->cap) {
        g->cap = (g->len + (size_t)n + 1) * 2;
        g->buf = realloc(g->buf, g->cap);
    }
    memcpy(g->buf + g->len, chunk, (size_t)n);
    g->len += (size_t)n;

    for (size_t i = 0; i < g->len; i++) {
        if (g->buf[i] != '\n')
            continue;
        g->buf[i] = '\0';
        if (i > start && g->buf[i - 1] == '\r')
            g->buf[i - 1] = '\0';
        on_line(g, g->buf + start);
        start = i + 1;
        if (g->from_gdb < 0)
            return;
    }
    memmove(g->buf, g->buf + start, g->len - start);
    g->len -= start;
}

static int gdbmi_fd(dbg_backend *b)
{
    return ((struct gdbmi *)b)->from_gdb;
}

/* Quote an argument for the shell GDB starts the program with. */
static void shell_quote(const char *s, char *out, size_t size)
{
    size_t n = 0;

    if (size < 3)
        return;
    out[n++] = '\'';
    for (; *s && n + 5 < size; s++) {
        if (*s == '\'') {
            memcpy(out + n, "'\\''", 4);
            n += 4;
        } else {
            out[n++] = *s;
        }
    }
    out[n++] = '\'';
    out[n] = '\0';
}

static void run(struct gdbmi *g)
{
    char q[600];

    if (g->run_to[0]) {
        char loc[300];
        snprintf(loc, sizeof loc, "*%s", g->run_to);
        mi_quote(loc, q, sizeof q);
        sendf(g, DBG_REQ_OTHER, NULL, 0, "-break-insert -t %s", q);
    }
    sendf(g, DBG_REQ_EXEC, NULL, 0, "-exec-run");
}

/* Write dbxl.py to a private directory and have GDB source it. */
static void load_helper(struct gdbmi *g)
{
    char path[128], cmd[160], q[400];
    FILE *f;

    snprintf(g->helper_dir, sizeof g->helper_dir, "%s/dbxl-XXXXXX",
             getenv("TMPDIR") && *getenv("TMPDIR") && strlen(getenv("TMPDIR")) < 40
                 ? getenv("TMPDIR") : "/tmp");
    if (!mkdtemp(g->helper_dir)) {
        g->helper_dir[0] = '\0';
        return;
    }
    snprintf(path, sizeof path, "%s/dbxl.py", g->helper_dir);
    f = fopen(path, "w");
    if (!f)
        return;
    for (int i = 0; dbxl_py[i]; i++)
        fputs(dbxl_py[i], f);
    fclose(f);
    snprintf(cmd, sizeof cmd, "source %s", path);
    mi_quote(cmd, q, sizeof q);
    sendf(g, DBG_REQ_OTHER, NULL, 0, "-interpreter-exec console %s", q);
}

static void remove_helper(struct gdbmi *g)
{
    char path[128];

    if (!g->helper_dir[0])
        return;
    snprintf(path, sizeof path, "%s/dbxl.py", g->helper_dir);
    unlink(path);
    rmdir(g->helper_dir);
    g->helper_dir[0] = '\0';
}

static int gdbmi_start(dbg_backend *b, const dbg_launch *l)
{
    struct gdbmi *g = (struct gdbmi *)b;
    int in[2], out[2];
    const char *gdb = getenv("DBXL_GDB");
    char q[4200], arg[2048];

    if (!gdb || !*gdb)
        gdb = "gdb";
    if (pipe(in) < 0)
        return -1;
    if (pipe(out) < 0) {
        close(in[0]);
        close(in[1]);
        return -1;
    }
    g->pid = fork();
    if (g->pid < 0)
        return -1;
    if (g->pid == 0) {
        int null = open("/dev/null", O_RDWR);

        dup2(in[0], 0);
        dup2(out[1], 1);
        if (null >= 0)
            dup2(null, 2);
        close(in[0]);
        close(in[1]);
        close(out[0]);
        close(out[1]);
        execlp(gdb, gdb, "--interpreter=mi3", "-nx", "-q", (char *)NULL);
        _exit(127);
    }
    close(in[0]);
    close(out[1]);
    g->to_gdb = in[1];
    g->from_gdb = out[0];
    fcntl(g->to_gdb, F_SETFD, FD_CLOEXEC);
    fcntl(g->from_gdb, F_SETFD, FD_CLOEXEC);

    copy(g->run_to, sizeof g->run_to, l->run_to);
    sendf(g, DBG_REQ_OTHER, NULL, 0, "-gdb-set confirm off");
    load_helper(g);
    mi_quote(l->program, q, sizeof q);
    sendf(g, DBG_REQ_START, NULL, 0, "-file-exec-and-symbols %s", q);
    {
        /*
         * `set args` hands the text to the startup shell as is, where
         * -exec-arguments (in recent GDB; seen with 17.1) quotes each word, which would
         * pass the redirections to the program as arguments.
         */
        char args[8192], tty[1100], cmd[8400];
        size_t n = 0;

        n += (size_t)snprintf(args, sizeof args, "set args");
        for (int i = 0; i < l->argc && n < sizeof args; i++) {
            shell_quote(l->argv[i], arg, sizeof arg);
            n += (size_t)snprintf(args + n, sizeof args - n, " %s", arg);
        }
        shell_quote(l->tty ? l->tty : "/dev/null", tty, sizeof tty);
        if (n < sizeof args)
            snprintf(args + n, sizeof args - n, " <%s >%s 2>&1", tty, tty);
        mi_quote(args, cmd, sizeof cmd);
        sendf(g, DBG_REQ_OTHER, NULL, 0, "-gdb-set startup-with-shell on");
        sendf(g, DBG_REQ_OTHER, NULL, 0, "-interpreter-exec console %s", cmd);
    }
    run(g);
    return 0;
}

static void gdbmi_shutdown(dbg_backend *b)
{
    struct gdbmi *g = (struct gdbmi *)b;

    if (g->to_gdb >= 0) {
        sendf(g, DBG_REQ_OTHER, NULL, 0, "-gdb-exit");
        close(g->to_gdb);
        g->to_gdb = -1;
    }
    if (g->from_gdb >= 0) {
        close(g->from_gdb);
        g->from_gdb = -1;
    }
    if (g->pid > 0) {
        /* Give GDB a moment to kill the program and leave. */
        for (int i = 0; i < 50; i++) {
            struct timespec ts = { 0, 20 * 1000 * 1000 };
            if (waitpid(g->pid, NULL, WNOHANG) != 0)
                break;
            nanosleep(&ts, NULL);
            if (i == 49) {
                kill(g->pid, SIGKILL);
                waitpid(g->pid, NULL, 0);
            }
        }
        g->pid = 0;
    }
    remove_helper(g);
    free(g->insns);
    free(g->regs);
    free(g->syms);
    free(g->threads);
    free(g->bytes);
    for (int i = 0; i < g->nnames; i++)
        free(g->names[i]);
    free(g->names);
    if (g->log)
        fclose(g->log);
    free(g->buf);
    free(g);
}

static void gdbmi_cont(dbg_backend *b)
{
    sendf((struct gdbmi *)b, DBG_REQ_EXEC, NULL, 0, "-exec-continue");
}

static void gdbmi_next(dbg_backend *b)
{
    sendf((struct gdbmi *)b, DBG_REQ_EXEC, NULL, 0, "-exec-next");
}

static void gdbmi_step(dbg_backend *b)
{
    sendf((struct gdbmi *)b, DBG_REQ_EXEC, NULL, 0, "-exec-step");
}

static void gdbmi_step_insn(dbg_backend *b)
{
    sendf((struct gdbmi *)b, DBG_REQ_EXEC, NULL, 0, "-exec-step-instruction");
}

static void gdbmi_finish(dbg_backend *b)
{
    sendf((struct gdbmi *)b, DBG_REQ_EXEC, NULL, 0, "-exec-finish");
}

static void gdbmi_restart(dbg_backend *b)
{
    run((struct gdbmi *)b);
}

static void gdbmi_bp_line(dbg_backend *b, const char *file, int line,
                          void *cookie)
{
    char loc[1100], q[2300];

    snprintf(loc, sizeof loc, "%s:%d", file, line);
    mi_quote(loc, q, sizeof q);
    sendf((struct gdbmi *)b, DBG_REQ_BP_LINE, cookie, 0, "-break-insert %s", q);
}

static void gdbmi_bp_func(dbg_backend *b, const char *func, void *cookie)
{
    char loc[300], q[700];

    /* At the function's first instruction, like xldb (its `{` line). */
    snprintf(loc, sizeof loc, "*%s", func);
    mi_quote(loc, q, sizeof q);
    sendf((struct gdbmi *)b, DBG_REQ_BP_FUNC, cookie, 0, "-break-insert %s", q);
}

static void gdbmi_bp_delete(dbg_backend *b, int id, void *cookie)
{
    sendf((struct gdbmi *)b, DBG_REQ_BP_DELETE, cookie, id, "-break-delete %d", id);
}

static void gdbmi_frames(dbg_backend *b, int max)
{
    sendf((struct gdbmi *)b, DBG_REQ_FRAMES, NULL, 0,
          "-stack-list-frames 0 %d", max > 0 ? max - 1 : MAX_FRAMES - 1);
}

static void gdbmi_values(dbg_backend *b, enum dbg_scope scope, int level,
                         const char *const *deref, int nderef, unsigned flags,
                         void *cookie)
{
    struct gdbmi *g = (struct gdbmi *)b;
    size_t cap = 256, n = 0;
    char *args = malloc(cap), q[1100];

    n = (size_t)snprintf(args, cap, scope == DBG_SCOPE_LOCALS ? "locals %d%s"
                                                              : "globals",
                         level, flags & DBG_VALUES_ALL_BLOCKS ? " --all" : "");
    for (int i = 0; i < nderef; i++) {
        size_t need;

        mi_quote(deref[i], q, sizeof q);
        need = n + strlen(q) + 16;
        if (need > cap) {
            cap = need * 2;
            args = realloc(args, cap);
        }
        n += (size_t)snprintf(args + n, cap - n, " --deref %s", q);
    }
    if (n < 3500)
        sendf(g, DBG_REQ_VALUES, cookie, (int)scope * 1000 + level,
              "-dbxl-values %s", args);
    else                           /* too many paths: follow none */
        sendf(g, DBG_REQ_VALUES, cookie, (int)scope * 1000 + level,
              "-dbxl-values %s", scope == DBG_SCOPE_LOCALS ? "locals" : "globals");
    free(args);
}

static void gdbmi_assign(dbg_backend *b, const char *expr, const char *text,
                         void *cookie)
{
    char e[2200], q[4500];

    snprintf(e, sizeof e, "(%s)=(%s)", expr, text);
    mi_quote(e, q, sizeof q);
    sendf((struct gdbmi *)b, DBG_REQ_ASSIGN, cookie, 0,
          "-data-evaluate-expression %s", q);
}

static void gdbmi_bp_addr(dbg_backend *b, uint64_t addr, void *cookie)
{
    sendf((struct gdbmi *)b, DBG_REQ_BP_ADDR, cookie, 0,
          "-break-insert *0x%llx", (unsigned long long)addr);
}

static void gdbmi_bp_enable(dbg_backend *b, int id, bool on, void *cookie)
{
    sendf((struct gdbmi *)b, DBG_REQ_BP_ENABLE, cookie, id, "-break-%s %d",
          on ? "enable" : "disable", id);
}

static void gdbmi_bp_condition(dbg_backend *b, int id, const char *cond,
                               void *cookie)
{
    sendf((struct gdbmi *)b, DBG_REQ_BP_CONDITION, cookie, id,
          "-break-condition %d %s", id, cond ? cond : "");
}

static void gdbmi_disassemble(dbg_backend *b, uint64_t addr, void *cookie)
{
    sendf((struct gdbmi *)b, DBG_REQ_DISASM, cookie, 0,
          "-data-disassemble -a 0x%llx -- 0", (unsigned long long)addr);
}

static void gdbmi_registers(dbg_backend *b, int level, const char *const *names,
                            int n, void *cookie)
{
    char args[4096];
    size_t len = 0;

    args[0] = '\0';
    for (int i = 0; i < n && len < sizeof args - 40; i++)
        len += (size_t)snprintf(args + len, sizeof args - len, " %s", names[i]);
    sendf((struct gdbmi *)b, DBG_REQ_REGISTERS, cookie, 0,
          "-dbxl-registers %d%s", level, args);
}

static void gdbmi_symbols(dbg_backend *b, bool all, void *cookie)
{
    sendf((struct gdbmi *)b, DBG_REQ_SYMBOLS, cookie, 0, "-dbxl-symbols%s",
          all ? " --all" : "");
}

static void gdbmi_data_start(dbg_backend *b, void *cookie)
{
    sendf((struct gdbmi *)b, DBG_REQ_DATA_START, cookie, 0, "-dbxl-data-start");
}

static void gdbmi_read_memory(dbg_backend *b, uint64_t addr, int len,
                              void *cookie)
{
    sendf((struct gdbmi *)b, DBG_REQ_MEMORY, cookie, 0,
          "-data-read-memory-bytes 0x%llx %d", (unsigned long long)addr, len);
}

static void gdbmi_write_memory(dbg_backend *b, uint64_t addr,
                               const unsigned char *bytes, int len, void *cookie)
{
    char hex[1024];
    int n = 0;

    for (int i = 0; i < len && n < (int)sizeof hex - 3; i++)
        n += snprintf(hex + n, sizeof hex - (size_t)n, "%02x", bytes[i]);
    sendf((struct gdbmi *)b, DBG_REQ_WRITE_MEMORY, cookie, 0,
          "-data-write-memory-bytes 0x%llx %s", (unsigned long long)addr, hex);
}

static void gdbmi_threads(dbg_backend *b, void *cookie)
{
    sendf((struct gdbmi *)b, DBG_REQ_THREADS, cookie, 0, "-thread-info");
}

static void gdbmi_types(dbg_backend *b, void *cookie)
{
    sendf((struct gdbmi *)b, DBG_REQ_TYPES, cookie, 0, "-dbxl-types");
}

static int gdbmi_pid(dbg_backend *b)
{
    return ((struct gdbmi *)b)->inferior_pid;
}

static const struct dbg_backend_ops ops = {
    .start = gdbmi_start,
    .shutdown = gdbmi_shutdown,
    .fd = gdbmi_fd,
    .readable = gdbmi_readable,
    .cont = gdbmi_cont,
    .next = gdbmi_next,
    .step = gdbmi_step,
    .step_insn = gdbmi_step_insn,
    .finish = gdbmi_finish,
    .restart = gdbmi_restart,
    .bp_line = gdbmi_bp_line,
    .bp_func = gdbmi_bp_func,
    .bp_delete = gdbmi_bp_delete,
    .frames = gdbmi_frames,
    .pid = gdbmi_pid,
    .values = gdbmi_values,
    .assign = gdbmi_assign,
    .bp_addr = gdbmi_bp_addr,
    .bp_enable = gdbmi_bp_enable,
    .bp_condition = gdbmi_bp_condition,
    .disassemble = gdbmi_disassemble,
    .registers = gdbmi_registers,
    .symbols = gdbmi_symbols,
    .data_start = gdbmi_data_start,
    .read_memory = gdbmi_read_memory,
    .write_memory = gdbmi_write_memory,
    .threads = gdbmi_threads,
    .types = gdbmi_types,
};

dbg_backend *dbg_gdbmi_create(dbg_event_fn fn, void *arg)
{
    struct gdbmi *g = calloc(1, sizeof *g);

    g->base.ops = &ops;
    g->base.fn = fn;
    g->base.arg = arg;
    g->to_gdb = g->from_gdb = -1;
    if (getenv("DBXL_MI_LOG") && *getenv("DBXL_MI_LOG"))
        g->log = fopen(getenv("DBXL_MI_LOG"), "a");
    return &g->base;
}
