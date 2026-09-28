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
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "backend/backend.h"
#include "core/layout.h"
#include "core/options.h"
#include "core/source.h"
#include "browse.h"
#include "data.h"
#include "machine.h"
#include "debugger.h"
#include "ui.h"
#include "xtk/xtk.h"

#define MAX_BPS 256
#define MAX_CALLERS 256

enum state { ST_NONE, ST_STARTING, ST_RUNNING, ST_STOPPED, ST_EXITED, ST_DEAD };

struct bp {
    int id;
    bool enabled;
    char func[256];
    char file[512];
    char fullname[1024];
    int line;
    uint64_t addr;
    char cond[1300];              /* "a: +100" for a conditional one */
};

/* What a breakpoint request was for. */
enum { REQ_TOGGLE = 1, REQ_FUNCTION, REQ_QUIET, REQ_ADDR, REQ_COND_NEW,
       REQ_COND, REQ_ENABLE, REQ_DISABLE, REQ_LOAD };

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
    dbg_frame *frames;            /* the stack, for Callers selection */
    int nframes;
    char func[256];               /* name of the pending function bp */
    char pending_cond[2000];       /* the trigger being set: GDB condition */
    char pending_label[1300];      /* ... and its Breakpoints text */
    bool have_symbols;            /* Files/Subprograms requested */
    int bp_selected;              /* Breakpoints row with the menu open */
    char bp_file[1024];           /* the breakpoints file, last used */
    bool auto_loaded;             /* loadBreakpoints done */
} dbg;

static int watched_fd = -1;       /* the backend's descriptor */
static int tty_fd = -1;           /* dbxl's terminal, when it is in front */

/*
 * The program shares dbxl's terminal.  GDB starts it in its own process
 * group, so like a shell dbxl makes that group the terminal's foreground
 * while the program runs (so it can read, and Ctrl-C interrupts it) and
 * takes the terminal back when it stops.
 */
static void set_foreground(pid_t pgrp)
{
    sigset_t block, old;

    if (tty_fd < 0 || pgrp <= 0)
        return;
    /* A background process group gets SIGTTOU from tcsetpgrp. */
    sigemptyset(&block);
    sigaddset(&block, SIGTTOU);
    sigprocmask(SIG_BLOCK, &block, &old);
    tcsetpgrp(tty_fd, pgrp);
    sigprocmask(SIG_SETMASK, &old, NULL);
}

static void give_terminal(void)
{
    int pid = dbg.b ? dbg.b->ops->pid(dbg.b) : 0;

    if (pid > 0)
        set_foreground(getpgid(pid));
}

static void take_terminal(void)
{
    set_foreground(getpgrp());
}

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
    memset(bp, 0, sizeof *bp);
    bp->id = b->id;
    bp->enabled = b->enabled;
    snprintf(bp->func, sizeof bp->func, "%s", b->func);
    snprintf(bp->file, sizeof bp->file, "%s", b->file);
    snprintf(bp->fullname, sizeof bp->fullname, "%s", b->fullname);
    bp->line = b->line;
    bp->addr = b->addr;
}

static struct bp *bp_by_id(int id)
{
    for (int i = 0; i < dbg.nbps; i++)
        if (dbg.bps[i].id == id)
            return &dbg.bps[i];
    return NULL;
}

static unsigned char bp_mark(const struct bp *bp)
{
    if (!bp->enabled)
        return XTK_MARK_STOP_DISABLED;
    return bp->cond[0] ? XTK_MARK_STOP_COND : XTK_MARK_STOP;
}

static void bp_remove(int id)
{
    for (int i = 0; i < dbg.nbps; i++)
        if (dbg.bps[i].id == id) {
            /* Keep the order they were set in (the Breakpoints list). */
            memmove(&dbg.bps[i], &dbg.bps[i + 1],
                    (size_t)(dbg.nbps - i - 1) * sizeof dbg.bps[0]);
            dbg.nbps--;
            return;
        }
}

/* ---- Source ---------------------------------------------------------- */

/*
 * The Breakpoints window: `func [line N in file]`, then `  obj: trigger`
 * for a conditional one and `  Disabled` for a disabled one, in the order
 * they were set (recon pass 14).
 */
static void render_breakpoints(void)
{
    char (*text)[2400] = calloc((size_t)dbg.nbps + 1, sizeof *text);
    const char **lines = calloc((size_t)dbg.nbps + 1, sizeof *lines);

    for (int i = 0; i < dbg.nbps; i++) {
        const struct bp *bp = &dbg.bps[i];
        int n = snprintf(text[i], sizeof text[i], "%s [line %d in %s]",
                         bp->func[0] ? bp->func : "?", bp->line,
                         base_name(bp->file));
        if (bp->cond[0])
            n += snprintf(text[i] + n, sizeof text[i] - (size_t)n, "  %s",
                          bp->cond);
        if (!bp->enabled)
            snprintf(text[i] + n, sizeof text[i] - (size_t)n, "  Disabled");
        lines[i] = text[i];
    }
    xtk_pane_set_lines(pane(DBXL_W_BREAKPOINTS), lines, dbg.nbps);
    free(lines);
    free(text);
}

static void update_marks(void)
{
    int n = dbg.src ? dbg.src->nlines : 1;
    unsigned char *marks = calloc((size_t)(n > 0 ? n : 1), 1);

    render_breakpoints();
    dbxl_machine_marks_changed();
    if (dbg.src)
        for (int i = 0; i < dbg.nbps; i++) {
            const struct bp *bp = &dbg.bps[i];
            if (bp->line >= 1 && bp->line <= n &&
                same_file(bp, dbg.src_file, dbg.src_fullname))
                marks[bp->line - 1] |= bp_mark(bp);
        }
    if (dbg.arrow >= 1 && dbg.arrow <= n)
        marks[dbg.arrow - 1] |= XTK_MARK_ARROW;
    xtk_pane_set_marks(pane(DBXL_W_SOURCE), marks, n);
    free(marks);
}

/*
 * Show a frame's file.  At a stop the arrow goes on its line; for a frame
 * selected in Callers the arrow stays at the stop (if that is in the same
 * file) and the frame's line gets the cyan bar (recon pass 13).
 */
static void show_source(const dbg_frame *f, bool stop)
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
        if (!stop)
            dbg.arrow = dbg.state == ST_STOPPED &&
                        strcmp(dbg.frame.file, f->file) == 0 ? dbg.frame.line : 0;
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
    xtk_pane_set_selected(p, stop || !dbg.src ? -1 : f->line - 1);
    {
        int centre = stop || !dbg.src ? dbg.arrow : f->line;
        int top = centre - 1 - xtk_pane_rows(p) / 2;
        xtk_pane_set_top(p, top > 0 ? top : 0);
    }
    update_marks();
    /* Only a stop brings Source forward (Files, Subprograms and
     * Breakpoints just change what it shows), and xldb keeps an open
     * Disassembly in front of it (recon pass 14). */
    if (stop) {
        dbxl_ui_show_window(DBXL_W_SOURCE);
        if (xtk_pane_mapped(pane(DBXL_W_DISASSEMBLY)))
            xtk_pane_raise(pane(DBXL_W_DISASSEMBLY));
    }
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

    if (kind == REQ_COND_NEW) {
        /* A trigger on a line without a breakpoint: now add the condition.
         * Only this case says so (recon pass 16). */
        bp_add(&ev->bp);
        dbg.b->ops->bp_condition(dbg.b, ev->bp.id, dbg.pending_cond,
                                 (void *)(intptr_t)REQ_COND);
        update_marks();
        dbxl_ui_message("Breakpoint set");
        return;
    }
    if (kind == REQ_ADDR) {
        bp_add(&ev->bp);
        update_marks();
        dbxl_ui_message("Breakpoint set");
        return;
    }
    if (kind == REQ_LOAD || kind == REQ_QUIET) {
        /* Loaded from a file, or Breakpoint all Subprograms: silently. */
        bp_add(&ev->bp);
        update_marks();
        return;
    }
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
    show_source(&ev->frame, true);
    locals_title(&ev->frame);
    dbxl_data_refresh(ev->frame.file[0] ? ev->frame.func : "", 0);
    dbxl_machine_stopped(ev->frame.addr, ev->reason, ev->signame);
    if (!dbg.auto_loaded) {
        dbg.auto_loaded = true;
        /* loadBreakpoints (the help, "Saving and Restoring Breakpoints"). */
        if (dbg.cfg.load_breakpoints)
            dbxl_debug_load_breakpoints(dbxl_debug_breakpoints_file());
    }
    dbg.b->ops->frames(dbg.b, MAX_CALLERS);
    /* After the frames, so Callers isn't kept waiting. */
    if (!dbg.have_symbols) {
        dbg.have_symbols = true;
        dbg.b->ops->symbols(dbg.b, dbxl_opt.subprograms_all, NULL);
        dbg.b->ops->types(dbg.b, NULL);
    }
    switch (ev->reason) {
    case DBG_STOP_BREAKPOINT: {
        const struct bp *bp = bp_by_id(ev->bkptno);
        dbxl_ui_message(bp && bp->cond[0]
                        ? "Conditional breakpoint (condition is satisfied)."
                        : "Breakpoint encountered.");
        break;
    }
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
    case DBG_REQ_VALUES:
        break;
    case DBG_REQ_ASSIGN:
    case DBG_REQ_BP_CONDITION:
    case DBG_REQ_WRITE_MEMORY:
        error_message(ev->message);
        break;
    case DBG_REQ_DISASM:
    case DBG_REQ_REGISTERS:
    case DBG_REQ_SYMBOLS:
    case DBG_REQ_DATA_START:
    case DBG_REQ_MEMORY:
    case DBG_REQ_THREADS:
    case DBG_REQ_TYPES:
    case DBG_REQ_BP_ENABLE:
        break;
    case DBG_REQ_BP_ADDR:
        error_message(ev->message);
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
    if (ev->type == DBG_EV_STOPPED || ev->type == DBG_EV_EXITED ||
        ev->type == DBG_EV_SIGNALLED || ev->type == DBG_EV_DIED)
        take_terminal();
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
        dbg.nframes = 0;
        dbxl_data_clear_locals();
        dbxl_machine_exited();
        snprintf(msg, sizeof msg, "Program exited with return code %d.",
                 ev->exit_code);
        dbxl_ui_message(msg);
        break;
    case DBG_EV_SIGNALLED:
        set_state(ST_EXITED);
        clear_arrow();
        show_callers(NULL, 0);
        dbg.nframes = 0;
        dbxl_data_clear_locals();
        dbxl_machine_exited();
        dbg_signal_text(0, ev->signame, sig, sizeof sig);
        snprintf(msg, sizeof msg, "Program terminated by signal %s.", sig);
        dbxl_ui_message(msg);
        break;
    case DBG_EV_FRAMES:
        free(dbg.frames);
        dbg.frames = calloc((size_t)(ev->nframes ? ev->nframes : 1),
                            sizeof *dbg.frames);
        memcpy(dbg.frames, ev->frames, (size_t)ev->nframes * sizeof *dbg.frames);
        dbg.nframes = ev->nframes;
        show_callers(ev->frames, ev->nframes);
        break;
    case DBG_EV_VALUES:
        dbxl_data_values(ev->values);
        break;
    case DBG_EV_ASSIGNED:
        if (ev->request == DBG_REQ_BP_ENABLE || ev->request == DBG_REQ_BP_CONDITION) {
            struct bp *bp = bp_by_id(ev->bp.id);
            intptr_t kind = (intptr_t)ev->cookie;
            if (bp && kind == REQ_ENABLE)
                bp->enabled = true;
            else if (bp && kind == REQ_DISABLE)
                bp->enabled = false;
            else if (bp && kind == REQ_COND)
                snprintf(bp->cond, sizeof bp->cond, "%s", dbg.pending_label);
            update_marks();
        } else {
            dbxl_data_assigned();
            dbxl_machine_event(ev);
        }
        break;
    case DBG_EV_DISASM:
    case DBG_EV_REGISTERS:
    case DBG_EV_DATA_START:
    case DBG_EV_MEMORY:
    case DBG_EV_THREADS:
        dbxl_machine_event(ev);
        break;
    case DBG_EV_SYMBOLS:
        dbxl_browse_event(ev);
        break;
    case DBG_EV_TYPES:
        dbxl_data_types(ev->names, ev->nnames);
        break;
    case DBG_EV_BP_SET:
        on_bp_set(ev);
        break;
    case DBG_EV_BP_DELETED:
        bp_remove(ev->bp.id);
        update_marks();
        if ((intptr_t)ev->cookie == REQ_TOGGLE)
            dbxl_ui_message("Breakpoint removed");
        if (dbg.nbps == 0 || dbg.bp_selected >= dbg.nbps)
            dbg.bp_selected = -1;
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
    /* Hand the terminal over only when dbxl has it (not started with &). */
    if (tty) {
        tty_fd = open(tty, O_RDWR | O_NOCTTY | O_CLOEXEC);
        if (tty_fd >= 0 && tcgetpgrp(tty_fd) != getpgrp()) {
            close(tty_fd);
            tty_fd = -1;
        }
    }

    memset(&l, 0, sizeof l);
    l.program = cfg->program;
    l.argc = cfg->argc;
    l.argv = cfg->argv;
    l.run_to = cfg->run_to ? cfg->run_to : "main";
    l.tty = tty;
    dbg.b = dbg_gdbmi_create(on_event, NULL);
    dbxl_data_set_backend(dbg.b);
    dbg.bp_selected = -1;
    set_state(ST_STARTING);
    if (dbg_start(dbg.b, &l) < 0) {
        dbxl_data_set_backend(NULL);
        set_state(ST_DEAD);
        dbxl_ui_message("Unable to start gdb.");
        return;
    }
    watched_fd = dbg_fd(dbg.b);
    xtk_loop_add_fd(watched_fd, readable, NULL);
    dbxl_machine_set_backend(dbg.b);
}

void dbxl_debug_shutdown(void)
{
    if (!dbg.b)
        return;
    dbxl_data_set_backend(NULL);
    dbxl_machine_set_backend(NULL);
    dbg.shutting_down = true;
    take_terminal();
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
    give_terminal();
    switch (action) {
    case ACT_CONTINUE:     ops->cont(dbg.b); break;
    case ACT_NEXT:         ops->next(dbg.b); break;
    case ACT_STEP:         ops->step(dbg.b); break;
    case ACT_MACHINE_STEP: ops->step_insn(dbg.b); break;
    case ACT_RETURN:       ops->finish(dbg.b); break;
    default:               take_terminal(); return;
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

void dbxl_debug_select_frame(int level)
{
    const dbg_frame *f;

    if (dbg.state != ST_STOPPED || level < 0 || level >= dbg.nframes)
        return;
    f = &dbg.frames[level];
    xtk_pane_set_selected(pane(DBXL_W_CALLERS), level);
    show_source(f, false);
    locals_title(f);
    dbxl_data_refresh(f->file[0] ? f->func : "", level);
    dbxl_machine_select_frame(level, f->addr);
}

/* ---- Source locations, Disassembly and the Breakpoints window ------------ */

void dbxl_debug_show_location(const char *file, const char *fullname, int line,
                              bool from_top)
{
    dbg_frame f;

    memset(&f, 0, sizeof f);
    snprintf(f.file, sizeof f.file, "%s", file);
    snprintf(f.fullname, sizeof f.fullname, "%s", fullname);
    f.line = line;
    show_source(&f, false);
    if (from_top)
        xtk_pane_set_top(pane(DBXL_W_SOURCE), 0);
}

unsigned char dbxl_debug_addr_marks(uint64_t addr)
{
    unsigned char m = 0;

    for (int i = 0; i < dbg.nbps; i++)
        if (dbg.bps[i].addr == addr)
            m |= bp_mark(&dbg.bps[i]);
    return m;
}

void dbxl_debug_toggle_addr(uint64_t addr)
{
    /* Like a Source click, this drops the Callers highlight. */
    xtk_pane_set_selected(pane(DBXL_W_CALLERS), -1);
    if (!dbg.b)
        return;
    for (int i = 0; i < dbg.nbps; i++)
        if (dbg.bps[i].addr == addr) {
            dbg.b->ops->bp_delete(dbg.b, dbg.bps[i].id,
                                  (void *)(intptr_t)REQ_TOGGLE);
            return;
        }
    dbg.b->ops->bp_addr(dbg.b, addr, (void *)(intptr_t)REQ_ADDR);
}

enum { BPA_CLEAR, BPA_DISABLE, BPA_ENABLE, BPA_CLEAR_ALL, BPA_DISABLE_ALL,
       BPA_ENABLE_ALL, BPA_N };

static void unselect_breakpoint(void)
{
    dbg.bp_selected = -1;
    xtk_pane_set_selected(pane(DBXL_W_BREAKPOINTS), -1);
    xtk_pane_set_selected(pane(DBXL_W_SOURCE), -1);
}

static void clear_all_done(int button, const char *text, void *arg)
{
    (void)text;
    (void)arg;
    if (button == 0)
        for (int i = 0; i < dbg.nbps; i++)
            dbg.b->ops->bp_delete(dbg.b, dbg.bps[i].id,
                                  (void *)(intptr_t)REQ_QUIET);
    unselect_breakpoint();
}

static void bp_action(xtk_menu *m, int item, int button, void *arg)
{
    int ax, ay, sel = dbg.bp_selected;
    const struct bp *bp = sel >= 0 && sel < dbg.nbps ? &dbg.bps[sel] : NULL;

    (void)button;
    (void)arg;
    xtk_menu_anchor(m, &ax, &ay);
    /* The pointer goes back to the click, except for the confirm dialog,
     * which warps it itself. */
    xtk_menu_close(m, item != BPA_CLEAR_ALL);
    if (!dbg.b)
        return;
    switch (item) {
    case BPA_CLEAR:
        if (bp)
            dbg.b->ops->bp_delete(dbg.b, bp->id, (void *)(intptr_t)REQ_QUIET);
        break;
    case BPA_DISABLE:
    case BPA_ENABLE:
        if (bp)
            dbg.b->ops->bp_enable(dbg.b, bp->id, item == BPA_ENABLE,
                                  (void *)(intptr_t)(item == BPA_ENABLE
                                                     ? REQ_ENABLE : REQ_DISABLE));
        break;
    case BPA_CLEAR_ALL:
        /* Confirmed first; the highlights stay while it asks. */
        xtk_dialog_confirm("Clear ALL user breakpoints?", "yes", "no", ax, ay,
                           clear_all_done, NULL);
        return;
    case BPA_DISABLE_ALL:
    case BPA_ENABLE_ALL:
        for (int i = 0; i < dbg.nbps; i++)
            dbg.b->ops->bp_enable(dbg.b, dbg.bps[i].id, item == BPA_ENABLE_ALL,
                                  (void *)(intptr_t)(item == BPA_ENABLE_ALL
                                                     ? REQ_ENABLE : REQ_DISABLE));
        break;
    }
    unselect_breakpoint();
}

void dbxl_debug_breakpoints_click(const xtk_pane_event *e)
{
    static const char *const items[BPA_N] = {
        "Clear breakpoint", "Disable breakpoint", "Enable breakpoint",
        "Clear all breakpoints", "Disable all breakpoints",
        "Enable all breakpoints",
    };
    /* xldb's widths: 200 here, 112 for Storage (recon pass 14). */
    xtk_menu_spec spec = { "Select Breakpoint Action", items, BPA_N, 200, 0 };
    xtk_rect g = xtk_pane_geometry(pane(DBXL_W_BREAKPOINTS));
    const struct bp *bp;

    if (e->line < 0 || e->line >= dbg.nbps)
        return;
    bp = &dbg.bps[e->line];
    dbg.bp_selected = e->line;
    xtk_pane_set_selected(pane(DBXL_W_BREAKPOINTS), e->line);
    dbxl_debug_show_location(bp->file, bp->fullname, bp->line, false);
    /* The menu's corner goes at the Breakpoints pane's centre. */
    xtk_menu_open_at(&spec, g.x + g.w / 2, g.y + g.h / 2, e->fx, e->fy,
                     bp_action, NULL);
}

/* ---- conditional breakpoints ------------------------------------------------ */

static void trim(char *s)
{
    size_t n = strlen(s), i = 0;

    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t'))
        s[--n] = '\0';
    while (s[i] == ' ' || s[i] == '\t')
        i++;
    memmove(s, s + i, n - i + 1);
}

/*
 * A trigger (xldb's help): `X`, `!X`, `X..Y`, `!X..Y`, values in the
 * object's display style; a range compares signed for character, signed,
 * decimal and scientific styles and unsigned otherwise.  It goes on the
 * current line; an existing breakpoint there becomes conditional
 * (recon pass 14).
 */
void dbxl_debug_break_trigger(const char *label, const char *expr,
                              const char *shown, const char *trigger,
                              bool is_signed)
{
    char t[256], lhs[700], *dots;
    bool neg = false;
    struct bp *bp = NULL;

    if (!dbg.b || dbg.state != ST_STOPPED || !dbg.frame.file[0])
        return;
    /* Like any breakpoint change, this drops the Callers highlight. */
    xtk_pane_set_selected(pane(DBXL_W_CALLERS), -1);
    /* xldb then scrolls Source so the line sits 5 rows above the bottom,
     * not past the end of the file (recon pass 16). */
    {
        xtk_pane *p = pane(DBXL_W_SOURCE);
        int top = dbg.frame.line - 1 - (xtk_pane_rows(p) - 5);
        int last = xtk_pane_nlines(p) - xtk_pane_rows(p);

        if (top > last)
            top = last;
        xtk_pane_set_top(p, top > 0 ? top : 0);
    }
    snprintf(t, sizeof t, "%s", trigger);
    trim(t);
    if (t[0] == '!') {
        neg = true;
        memmove(t, t + 1, strlen(t));
        trim(t);
    }
    if (!t[0])
        return;
    if (is_signed)
        snprintf(lhs, sizeof lhs, "(%s)", expr);
    else
        snprintf(lhs, sizeof lhs, "(unsigned long long)(%s)", expr);
    dots = strstr(t, "..");
    if (dots) {
        *dots = '\0';
        trim(t);
        trim(dots + 2);
        snprintf(dbg.pending_cond, sizeof dbg.pending_cond,
                 "%s(%s >= (%s) && %s <= (%s))", neg ? "!" : "", lhs, t, lhs,
                 dots + 2);
    } else {
        snprintf(dbg.pending_cond, sizeof dbg.pending_cond, "%s %s (%s)", lhs,
                 neg ? "!=" : "==", t);
    }
    /* The entry shows the trigger as typed. */
    snprintf(dbg.pending_label, sizeof dbg.pending_label, "%s: %s", label,
             shown);

    for (int i = 0; i < dbg.nbps; i++)
        if (dbg.bps[i].line == dbg.frame.line &&
            same_file(&dbg.bps[i], dbg.frame.file, dbg.frame.fullname))
            bp = &dbg.bps[i];
    if (bp)
        dbg.b->ops->bp_condition(dbg.b, bp->id, dbg.pending_cond,
                                 (void *)(intptr_t)REQ_COND);
    else
        dbg.b->ops->bp_line(dbg.b, dbg.frame.fullname[0] ? dbg.frame.fullname
                                                         : dbg.frame.file,
                            dbg.frame.line, (void *)(intptr_t)REQ_COND_NEW);
}

/* ---- saving and loading breakpoints (recon pass 15) ---------------------- */

const char *dbxl_debug_breakpoints_file(void)
{
    if (!dbg.bp_file[0]) {
        const char *prog = dbg.cfg.program ? base_name(dbg.cfg.program) : "";
        if (dbg.cfg.breakpoints_file && dbg.cfg.breakpoints_file[0])
            snprintf(dbg.bp_file, sizeof dbg.bp_file, "%s",
                     dbg.cfg.breakpoints_file);
        else
            snprintf(dbg.bp_file, sizeof dbg.bp_file, ".dbxl.%s", prog);
    }
    return dbg.bp_file;
}

/* A source line's text, for the breakpoints file ("" if unknown). */
static const char *source_text(const struct bp *bp)
{
    char path[1024];
    const dbxl_source *src;

    if (dbxl_source_find(bp->file, bp->fullname, dbxl_opt.source_path,
                         dbg.cfg.program, path, sizeof path) != 0)
        return "";
    src = dbxl_source_load(path);
    return src && bp->line >= 1 && bp->line <= src->nlines
           ? src->lines[bp->line - 1] : "";
}

/* One line per breakpoint: `break <file> <file> <line> <source line>`. */
void dbxl_debug_save_breakpoints(const char *file)
{
    FILE *f = fopen(file, "w");
    char msg[1100];

    snprintf(dbg.bp_file, sizeof dbg.bp_file, "%s", file);
    if (!f) {
        snprintf(msg, sizeof msg, "Unable to open %s", file);
        dbxl_ui_message(msg);
        return;
    }
    for (int i = 0; i < dbg.nbps; i++) {
        const struct bp *bp = &dbg.bps[i];
        if (!bp->file[0] || bp->line < 1)
            continue;
        fprintf(f, "break %s %s %d %s\n", bp->file, bp->file, bp->line,
                source_text(bp));
    }
    fclose(f);
}

/* Adds the file's breakpoints, skipping ones already set. */
void dbxl_debug_load_breakpoints(const char *file)
{
    FILE *f;
    char line[2048], msg[1100];

    snprintf(dbg.bp_file, sizeof dbg.bp_file, "%s", file);
    if (!dbg.b)
        return;
    f = fopen(file, "r");
    if (!f) {
        snprintf(msg, sizeof msg, "Unable to open %s", file);
        dbxl_ui_message(msg);
        return;
    }
    while (fgets(line, sizeof line, f)) {
        char src[1024], obj[1024];
        int n;
        bool have = false;

        if (sscanf(line, "break %1023s %1023s %d", src, obj, &n) != 3 || n < 1)
            continue;
        for (int i = 0; i < dbg.nbps; i++)
            if (dbg.bps[i].line == n && same_file(&dbg.bps[i], src, NULL))
                have = true;
        if (!have)
            dbg.b->ops->bp_line(dbg.b, src, n, (void *)(intptr_t)REQ_LOAD);
    }
    fclose(f);
}

/* Breakpoint all Subprograms: every function with debug information, in
 * address order, silently (recon pass 15). */
static int by_addr(const void *a, const void *b)
{
    const dbg_symbol *x = a, *y = b;

    return x->addr < y->addr ? -1 : x->addr > y->addr;
}

void dbxl_debug_break_all(void)
{
    const dbg_symbol *listed;
    int n = dbxl_browse_functions(&listed);
    dbg_symbol *funcs;

    if (!dbg.b || n <= 0)
        return;
    funcs = malloc((size_t)n * sizeof *funcs);
    memcpy(funcs, listed, (size_t)n * sizeof *funcs);
    qsort(funcs, (size_t)n, sizeof *funcs, by_addr);
    for (int i = 0; i < n; i++) {
        bool have = false;

        if (!funcs[i].file[0])
            continue;
        for (int k = 0; k < dbg.nbps; k++)
            if (dbg.bps[k].addr == funcs[i].addr ||
                (dbg.bps[k].line == funcs[i].line &&
                 same_file(&dbg.bps[k], funcs[i].file, funcs[i].fullname)))
                have = true;
        if (!have)
            dbg.b->ops->bp_func(dbg.b, funcs[i].name, (void *)(intptr_t)REQ_QUIET);
    }
    free(funcs);
}

void dbxl_debug_options_changed(int item)
{
    if (!dbg.b)
        return;
    if (item == OPT_SUBPROGRAMS)
        dbg.b->ops->symbols(dbg.b, dbxl_opt.subprograms_all, NULL);
    else if (item == OPT_LOCAL_VARIABLES && dbg.state == ST_STOPPED)
        dbxl_data_refresh(dbg.frame.file[0] ? dbg.frame.func : "", 0);
}
