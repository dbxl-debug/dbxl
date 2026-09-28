/*
 * The debugging controller: runs the program under a backend and shows
 * its state in the panes (Source, Callers, Locals title, Messages).
 */
#ifndef DBXL_DEBUGGER_H
#define DBXL_DEBUGGER_H

#include <stdbool.h>

#include "core/commandlist.h"

struct dbxl_debug_config {
    const char *program;          /* NULL: no program */
    int argc;
    char **argv;
    const char *run_to;           /* "main" */
    const char *edit;             /* edit command, %d line, %s file */
    const char *const *include;   /* -I directories, appended to the path */
    int ninclude;
};

void dbxl_debug_init(const struct dbxl_debug_config *cfg);
void dbxl_debug_shutdown(void);
bool dbxl_debug_active(void);

/* Continue, Next, Step, Machine step, Return, Restart, Edit. */
void dbxl_debug_command(enum dbxl_action action);
/* A left click on Source line `line` (1-based): toggle a breakpoint. */
void dbxl_debug_source_click(int line);
/* A left click on Callers line `level`: show that frame. */
void dbxl_debug_select_frame(int level);
/* The Breakpoint dialog: a breakpoint at a function's entry. */
void dbxl_debug_break_function(const char *name);

#endif
