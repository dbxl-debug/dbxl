/*
 * The Options menu's settings and xldb's formatting of its items
 * (recon passes 1, 6, 11).
 */
#ifndef DBXL_OPTIONS_H
#define DBXL_OPTIONS_H

#include <stdbool.h>

enum dbxl_option_item {
    OPT_SAVE_LAYOUT,
    OPT_SOURCE_PATH,
    OPT_LOCAL_VARIABLES,
    OPT_DETAIL_PER_CLICK,
    OPT_MULTIPROCESS,
    OPT_FORK_PATH,
    OPT_AUTORAISE,
    OPT_COMPILER_VARIABLES,
    OPT_LANGUAGE,
    OPT_REGISTER_CONTROL,
    OPT_SUBPROGRAMS,
    OPT_SAVE_BREAKPOINTS,
    OPT_LOAD_BREAKPOINTS,
    OPT_CASE_SENSITIVE,
    OPT_BREAK_ALL,
    OPT_NITEMS
};

enum dbxl_register_item { REG_GENERAL, REG_DOUBLE, REG_SPECIAL, REG_NITEMS };

enum { FORK_PARENT, FORK_CHILD, FORK_BOTH };

struct dbxl_options {
    bool local_all;          /* Local variables: Active / All */
    int detail_per_click;
    bool multiprocess;
    int fork_path;           /* FORK_PARENT, FORK_CHILD, FORK_BOTH */
    bool autoraise;
    bool compiler_vars;
    bool subprograms_all;
    bool case_sensitive;
    bool break_all;
    bool reg[REG_NITEMS];
    char source_path[256];
};

extern struct dbxl_options dbxl_opt;

void dbxl_options_defaults(void);
/* Format item k of the Options / Display controls menus into buf. */
void dbxl_options_item(int k, char *buf, int size);
void dbxl_options_register_item(int k, char *buf, int size);
/* Cycle a value: forward = left button, backward = right button. */
bool dbxl_options_cycle(int k, bool forward);
void dbxl_options_register_cycle(int k);

#define DBXL_OPTIONS_MENU_WIDTH 288   /* observed; wider than any item */

#endif
