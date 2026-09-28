/*
 * The debugging controller: runs the program under a backend and shows
 * its state in the panes (Source, Callers, Locals title, Messages).
 */
#ifndef DBXL_DEBUGGER_H
#define DBXL_DEBUGGER_H

#include <stdbool.h>

#include <stdint.h>

#include "core/commandlist.h"
#include "xtk/xtk.h"

struct dbxl_debug_config {
    const char *program;          /* NULL: no program */
    int argc;
    char **argv;
    const char *run_to;           /* "main" */
    const char *edit;             /* edit command, %d line, %s file */
    const char *const *include;   /* -I directories, appended to the path */
    int ninclude;
    const char *breakpoints_file; /* the breakpointsFile resource */
    bool load_breakpoints;        /* loadBreakpoints (and not -k) */
    bool quiet;                   /* -q: run to the first signal, then map */
    const char *core;             /* -co: the core file ("core") */
    int attach_pid;               /* -a */
    int max_calls;                /* -c / maxCalls */
    char (*ignore)[16];           /* -i / ignoreSignals, GDB names */
    int nignore;
    bool no_shared;               /* -n / ignoreSharedObjects */
    bool verbose;                 /* -v */
    const char *fetch_source;     /* -F / fetchSource */
    bool automatic_breakpoints;   /* automaticBreakpoints */
};

void dbxl_debug_init(const struct dbxl_debug_config *cfg);
/* dbxl's exit status: the program's return code if it exited normally,
 * else 255. */
int dbxl_debug_exit_status(void);
/* Debugging a core file (Commands shows only Edit, Exit, Options, Help;
 * modifications are refused). */
bool dbxl_debug_core(void);
/* The program has ended (exited or killed by a signal). */
bool dbxl_debug_terminated(void);
/* The Signal command. */
void dbxl_debug_signal(void);
/* A user command's `func()`: call it with the Function parameter. */
void dbxl_debug_call(const char *func, uint64_t arg, bool has_arg);
/* The Options menu changed Multiprocess debugging or Fork path. */
void dbxl_debug_fork_mode_changed(void);
void dbxl_debug_shutdown(void);
bool dbxl_debug_active(void);

/* Continue, Next, Step, Machine step, Return, Restart, Edit. */
void dbxl_debug_command(enum dbxl_action action);
/* A left click on Source line `line` (1-based): toggle a breakpoint. */
void dbxl_debug_source_click(int line);
/* A left click on Callers line `level`: show that frame. */
void dbxl_debug_select_frame(int level);
/* Show a file in Source with `line` in cyan: centred, or from line 1. */
void dbxl_debug_show_location(const char *file, const char *fullname, int line,
                              bool from_top);
/* Margin glyphs (XTK_MARK_*) for breakpoints at an instruction. */
unsigned char dbxl_debug_addr_marks(uint64_t addr);
/* A Disassembly click: set or clear a breakpoint at an instruction. */
void dbxl_debug_toggle_addr(uint64_t addr);
/* A click in the Breakpoints window. */
void dbxl_debug_breakpoints_click(const xtk_pane_event *e);
/* A variable menu's Breakpoint: a trigger on the current line.  shown is
 * the trigger as typed, trigger the same with each value as a literal. */
void dbxl_debug_break_trigger(const char *label, const char *expr,
                              const char *shown, const char *trigger,
                              bool is_signed);
/* Options: the breakpoints file (default .dbxl.<program>), saving and
 * loading it, Breakpoint all Subprograms, and options that refetch. */
const char *dbxl_debug_breakpoints_file(void);
void dbxl_debug_save_breakpoints(const char *file);
void dbxl_debug_load_breakpoints(const char *file);
void dbxl_debug_break_all(void);
void dbxl_debug_options_changed(int item);
/* The Breakpoint dialog: a breakpoint at a function's entry. */
void dbxl_debug_break_function(const char *name);

#endif
