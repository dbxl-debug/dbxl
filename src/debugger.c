/*
 * The debugging controller (milestone 3, recon pass 12).
 *
 * xldb's behaviour on a stop: the Source pane shows the frame's file with
 * the arrow on the stop line and is scrolled so that line sits half a page
 * down (top = line - rows/2, at least the first line), whether or not it
 * was already visible; the Callers pane lists the stack as "func()" with
 * the current frame highlighted; the Locals title names the function and
 * file.  Messages come from xldb's catalogue (recon/xldb-messages.txt).
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "backend/backend.h"
#include "core/layout.h"
#include "core/options.h"
#include "core/source.h"
#include "debugger.h"
#include "ui.h"
#include "xtk/xtk.h"

#define MAX_BPS 256
#define MAX_CALLERS 256

enum state { ST_NONE, ST_STARTING, ST_RUNNING, ST_STOPPED, ST_EXITED, ST_DEAD };

struct bp {
    int id;
    bool enabled;
    char file[512];
    char fullname[1024];
    int line;
};

/* What a breakpoint request was for. */
enum { REQ_TOGGLE = 1, REQ_FUNCTION, REQ_QUIET };

static struct {
    struct dbxl_debug_config cfg;
    dbg_backend *b;
    enum state state;
    bool shutting_down;
    bool load_failed;             /* -file-exec-and-symbols failed */
    dbg_frame frame;              /* where the program stopped */
    const dbxl_source *src;       /* shown in Source, NULL = none */
    char src_file[512];           /* the frame's file as compiled */
    char src_fullname[1024];
    int arrow;                    /* 1-based line of the arrow, 0 = none */
    struct bp bps[MAX_BPS];
    int nbps;
    int click_line;               /* line of the pending toggle */
    char func[256];               /* name of the pending function bp */
} dbg;

static int watched_fd = -1;       /* the backend's descriptor */

static xtk_pane *pane(int w) { return dbxl_ui_pane(w); }

static const char *base_name(const char *path)
{
    const char *s = strrchr(path, '/');

    return s ? s + 1 : path;
}

/* ---- breakpoint table ------------------------------------------------ */

static bool same_file(const struct bp *bp, const char *file, const char *fullname)
{
    if (bp->fullname[0] && fullname && fullname[0])
        return strcmp(bp->fullname, fullname) == 0;
    return file && strcmp(base_name(bp->file), base_name(file)) == 0;
}

static struct bp *bp_at(int line)
{
    for (int i = 0; i < dbg.nbps; i++)
        if (dbg.bps[i].line == line &&
            same_file(&dbg.bps[i], dbg.src_file, dbg.src_fullname))
            return &dbg.bps[i];
    return NULL;
}

static void bp_add(const dbg_breakpoint *b)
{
    struct bp *bp;

    if (dbg.nbps == MAX_BPS)
        return;
    bp = &dbg.bps[dbg.nbps++];
    bp->id = b->id;
    bp->enabled = b->enabled;
    snprintf(bp->file, sizeof bp->file, "%s", b->file);
    snprintf(bp->fullname, sizeof bp->fullname, "%s", b->fullname);
    bp->line = b->line;
}

static void bp_remove(int id)
{
    for (int i = 0; i < dbg.nbps; i++)
        if (dbg.bps[i].id == id) {
            dbg.bps[i] = dbg.bps[--dbg.nbps];
            return;
        }
}

/* ---- Source ---------------------------------------------------------- */

static void update_marks(void)
{
    int n = dbg.src ? dbg.src->nlines : 1;
    unsigned char *marks = calloc((size_t)(n > 0 ? n : 1), 1);

    if (dbg.src)
        for (int i = 0; i < dbg.nbps; i++) {
            const struct bp *bp = &dbg.bps[i];
            if (bp->line >= 1 && bp->line <= n &&
                same_file(bp, dbg.src_file, dbg.src_fullname))
                marks[bp->line - 1] |= bp->enabled ? XTK_MARK_STOP
                                                   : XTK_MARK_STOP_DISABLED;
        }
    if (dbg.arrow >= 1 && dbg.arrow <= n)
        marks[dbg.arrow - 1] |= XTK_MARK_ARROW;
    xtk_pane_set_marks(pane(DBXL_W_SOURCE), marks, n);
    free(marks);
}

static void show_source(const dbg_frame *f)
{
    xtk_pane *p = pane(DBXL_W_SOURCE);
    char path[1024];
    const char *title;

    snprintf(dbg.src_file, sizeof dbg.src_file, "%s", f->file);
    snprintf(dbg.src_fullname, sizeof dbg.src_fullname, "%s", f->fullname);
    dbg.src = NULL;
    if (f->file[0] &&
        dbxl_source_find(f->file, f->fullname, dbxl_opt.source_path,
                         dbg.cfg.program, path, sizeof path) == 0)
        dbg.src = dbxl_source_load(path);

    if (dbg.src) {
        xtk_pane_set_lines(p, (const char *const *)dbg.src->lines,
                           dbg.src->nlines);
        title = dbg.src->path;
        dbg.arrow = f->line;
    } else {
        /*
         * No source: xldb titles the pane with the file name it has (for
         * library code, the assembler file) and puts the arrow on the
         * first row of an empty view.
         */
        xtk_pane_set_lines(p, NULL, 0);
        title = f->file[0] ? f->file : f->from[0] ? base_name(f->from)
                                                  : f->func;
        dbg.arrow = 1;
        if (f->file[0])
            dbxl_ui_message("The source file was not found in the source "
                            "search path.");
    }
    xtk_pane_set_title(p, title);
    {
        int top = dbg.arrow - 1 - xtk_pane_rows(p) / 2;
        xtk_pane_set_top(p, top > 0 ? top : 0);
    }
    update_marks();
    dbxl_ui_show_window(DBXL_W_SOURCE);
}

static void clear_arrow(void)
{
    dbg.arrow = 0;
    update_marks();
}

/* ---- Callers and Locals --------------------------------------------- */

static void show_callers(const dbg_frame *frames, int n)
{
    xtk_pane *p = pane(DBXL_W_CALLERS);
    char (*text)[300] = calloc((size_t)(n > 0 ? n : 1), sizeof *text);
    const char **lines = calloc((size_t)(n > 0 ? n : 1), sizeof *lines);

    for (int i = 0; i < n; i++) {
        if (frames[i].func[0])
            snprintf(text[i], sizeof text[i], "%s()", frames[i].func);
        else
            snprintf(text[i], sizeof text[i], "0x%llx()",
                     (unsigned long long)frames[i].addr);
        lines[i] = text[i];
    }
    xtk_pane_set_lines(p, lines, n);
    xtk_pane_set_selected(p, n > 0 ? 0 : -1);
    free(lines);
    free(text);
}

static void locals_title(const dbg_frame *f)
{
    char title[1024];

    if (f->file[0])
        snprintf(title, sizeof title, "Locals for %s() in %s",
                 f->func[0] ? f->func : "[unknown]", base_name(f->file));
    else
        snprintf(title, sizeof title, "Locals for %s()",
                 f->func[0] ? f->func : "[unknown]");
    xtk_pane_set_title(pane(DBXL_W_LOCALS), title);
}

/* ---- backend events -------------------------------------------------- */

static void set_state(enum state s)
{
    dbg.state = s;
    xtk_frame_busy(s == ST_STARTING || s == ST_RUNNING);
}

static void cannot_breakpoint(int line)
{
    char msg[700];

    snprintf(msg, sizeof msg, "Cannot breakpoint line %d of `%s'", line,
             base_name(dbg.src_file));
    dbxl_ui_message(msg);
}

static void on_bp_set(const dbg_event *ev)
{
    intptr_t kind = (intptr_t)ev->cookie;
    char msg[128];

    if (kind == REQ_TOGGLE && ev->bp.line != dbg.click_line) {
        /* GDB moved it to the next line with code; xldb refuses. */
        dbg.b->ops->bp_delete(dbg.b, ev->bp.id, (void *)(intptr_t)REQ_QUIET);
        cannot_breakpoint(dbg.click_line);
        return;
    }
    bp_add(&ev->bp);
    update_marks();
    if (kind == REQ_TOGGLE) {
        if (ev->bp.nlocations > 1)
            snprintf(msg, sizeof msg, "%d breakpoints set", ev->bp.nlocations);
        else
            snprintf(msg, sizeof msg, "Breakpoint set");
        dbxl_ui_message(msg);
    }
}

static void on_stopped(const dbg_event *ev)
{
    char sig[128], msg[256];

    set_state(ST_STOPPED);
    dbg.frame = ev->frame;
    show_source(&ev->frame);
    locals_title(&ev->frame);
    dbg.b->ops->frames(dbg.b, MAX_CALLERS);
    switch (ev->reason) {
    case DBG_STOP_BREAKPOINT:
        dbxl_ui_message("Breakpoint encountered.");
        break;
    case DBG_STOP_SIGNAL:
        dbg_signal_text(0, ev->signame, sig, sizeof sig);
        snprintf(msg, sizeof msg, "Program stopped during trace by signal %s.",
                 sig);
        dbxl_ui_message(msg);
        break;
    default:
        break;
    }
}

/* GDB's errors can span lines; the Messages bar shows the first. */
static void error_message(const char *text)
{
    char msg[512];

    snprintf(msg, sizeof msg, "%.*s", (int)strcspn(text, "\n"), text);
    dbxl_ui_message(msg);
}

static void on_error(const dbg_event *ev)
{
    char msg[512];

    switch (ev->request) {
    case DBG_REQ_BP_LINE:
        cannot_breakpoint(dbg.click_line);
        break;
    case DBG_REQ_BP_FUNC:
        snprintf(msg, sizeof msg,
                 "Subprogram '%s' not found (perhaps it was not compiled "
                 "with -g).", dbg.func);
        dbxl_ui_message(msg);
        break;
    case DBG_REQ_BP_DELETE:
    case DBG_REQ_FRAMES:
        break;
    case DBG_REQ_START:
        /* The program could not be loaded; what follows fails too. */
        dbg.load_failed = true;
        set_state(ST_EXITED);
        error_message(ev->message);
        break;
    default:
        /* e.g. "The program is not being run." */
        if (dbg.state == ST_RUNNING || dbg.state == ST_STARTING)
            set_state(dbg.arrow ? ST_STOPPED : ST_EXITED);
        if (!dbg.load_failed)
            error_message(ev->message);
        break;
    }
}

static void on_event(const dbg_event *ev, void *arg)
{
    char sig[128], msg[256];

    (void)arg;
    switch (ev->type) {
    case DBG_EV_RUNNING:
        if (dbg.state != ST_STARTING)
            set_state(ST_RUNNING);
        break;
    case DBG_EV_STOPPED:
        on_stopped(ev);
        break;
    case DBG_EV_EXITED:
        set_state(ST_EXITED);
        clear_arrow();
        show_callers(NULL, 0);
        snprintf(msg, sizeof msg, "Program exited with return code %d.",
                 ev->exit_code);
        dbxl_ui_message(msg);
        break;
    case DBG_EV_SIGNALLED:
        set_state(ST_EXITED);
        clear_arrow();
        show_callers(NULL, 0);
        dbg_signal_text(0, ev->signame, sig, sizeof sig);
        snprintf(msg, sizeof msg, "Program terminated by signal %s.", sig);
        dbxl_ui_message(msg);
        break;
    case DBG_EV_FRAMES:
        show_callers(ev->frames, ev->nframes);
        break;
    case DBG_EV_BP_SET:
        on_bp_set(ev);
        break;
    case DBG_EV_BP_DELETED:
        bp_remove(ev->bp.id);
        update_marks();
        if ((intptr_t)ev->cookie == REQ_TOGGLE)
            dbxl_ui_message("Breakpoint removed");
        break;
    case DBG_EV_ERROR:
        on_error(ev);
        break;
    case DBG_EV_DIED:
        if (watched_fd >= 0) {
            xtk_loop_remove_fd(watched_fd);
            watched_fd = -1;
        }
        set_state(ST_DEAD);
        if (!dbg.shutting_down)
            dbxl_ui_message("The debugger (gdb) has exited.");
        break;
    }
}

static void readable(int fd, void *arg)
{
    (void)fd;
    (void)arg;
    dbg_readable(dbg.b);
}

/* ---- public ---------------------------------------------------------- */

void dbxl_debug_init(const struct dbxl_debug_config *cfg)
{
    dbg_launch l;
    const char *tty = NULL;

    dbg.cfg = *cfg;
    for (int i = 0; i < cfg->ninclude; i++) {
        size_t n = strlen(dbxl_opt.source_path);
        snprintf(dbxl_opt.source_path + n, sizeof dbxl_opt.source_path - n,
                 "%s%s ", n && dbxl_opt.source_path[n - 1] != ' ' ? " " : "",
                 cfg->include[i]);
    }
    if (!cfg->program)
        return;
    for (int fd = 0; fd <= 2 && !tty; fd++)
        if (isatty(fd))
            tty = ttyname(fd);

    memset(&l, 0, sizeof l);
    l.program = cfg->program;
    l.argc = cfg->argc;
    l.argv = cfg->argv;
    l.run_to = cfg->run_to ? cfg->run_to : "main";
    l.tty = tty;
    dbg.b = dbg_gdbmi_create(on_event, NULL);
    set_state(ST_STARTING);
    if (dbg_start(dbg.b, &l) < 0) {
        set_state(ST_DEAD);
        dbxl_ui_message("Unable to start gdb.");
        return;
    }
    watched_fd = dbg_fd(dbg.b);
    xtk_loop_add_fd(watched_fd, readable, NULL);
}

void dbxl_debug_shutdown(void)
{
    if (!dbg.b)
        return;
    dbg.shutting_down = true;
    if (watched_fd >= 0)
        xtk_loop_remove_fd(watched_fd);
    dbg_shutdown(dbg.b);
    dbg.b = NULL;
}

bool dbxl_debug_active(void)
{
    return dbg.b != NULL;
}

static void edit(void)
{
    const char *fmt = dbg.cfg.edit ? dbg.cfg.edit : "xterm -n vi -e vi +%d %s &";
    char cmd[4096];
    size_t n = 0;

    if (!dbg.src)
        return;
    for (const char *p = fmt; *p && n < sizeof cmd - 1; p++) {
        if (p[0] == '%' && p[1] == 'd') {
            n += (size_t)snprintf(cmd + n, sizeof cmd - n, "%d",
                                  dbg.arrow > 0 ? dbg.arrow : 1);
            p++;
        } else if (p[0] == '%' && p[1] == 's') {
            n += (size_t)snprintf(cmd + n, sizeof cmd - n, "%s", dbg.src->path);
            p++;
        } else {
            cmd[n++] = *p;
        }
        if (n >= sizeof cmd)
            return;
    }
    cmd[n] = '\0';
    if (system(cmd) < 0)
        dbxl_ui_message("Unable to run the edit command.");
}

void dbxl_debug_command(enum dbxl_action action)
{
    const struct dbg_backend_ops *ops;

    if (action == ACT_EDIT) {
        edit();
        return;
    }
    if (!dbg.b || dbg.state == ST_DEAD)
        return;
    ops = dbg.b->ops;
    if (action == ACT_RESTART) {
        if (dbg.state == ST_RUNNING || dbg.state == ST_STARTING)
            return;
        set_state(ST_STARTING);
        ops->restart(dbg.b);
        return;
    }
    if (dbg.state != ST_STOPPED && dbg.state != ST_EXITED)
        return;
    switch (action) {
    case ACT_CONTINUE:     ops->cont(dbg.b); break;
    case ACT_NEXT:         ops->next(dbg.b); break;
    case ACT_STEP:         ops->step(dbg.b); break;
    case ACT_MACHINE_STEP: ops->step_insn(dbg.b); break;
    case ACT_RETURN:       ops->finish(dbg.b); break;
    default:               return;
    }
    if (dbg.state == ST_STOPPED)
        set_state(ST_RUNNING);
}

void dbxl_debug_source_click(int line)
{
    struct bp *bp;

    /* xldb drops the Callers highlight on a Source click (recon pass 12). */
    xtk_pane_set_selected(pane(DBXL_W_CALLERS), -1);
    if (!dbg.b || !dbg.src || line < 1 || line > dbg.src->nlines) {
        cannot_breakpoint(line);
        return;
    }
    bp = bp_at(line);
    if (bp) {
        dbg.b->ops->bp_delete(dbg.b, bp->id, (void *)(intptr_t)REQ_TOGGLE);
        return;
    }
    dbg.click_line = line;
    dbg.b->ops->bp_line(dbg.b, dbg.src_fullname[0] ? dbg.src_fullname
                                                   : dbg.src_file,
                        line, (void *)(intptr_t)REQ_TOGGLE);
}

void dbxl_debug_break_function(const char *name)
{
    if (!dbg.b || !name[0])
        return;
    snprintf(dbg.func, sizeof dbg.func, "%s", name);
    dbg.b->ops->bp_func(dbg.b, name, (void *)(intptr_t)REQ_FUNCTION);
}
