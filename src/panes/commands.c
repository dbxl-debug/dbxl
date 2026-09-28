/*
 * The Commands pane.
 *
 * xldb's compiled-in default commandList (recon pass 1):
 *   "Continue         "=continue:c ... "Help             "=help:h ""=null
 * Each name is 17 columns wide and is followed by "Alt-<key>".  The
 * commandList resource parser replaces this table in a later milestone.
 */
#include <stdio.h>

#include "panes/commands.h"

static const struct {
    const char *name;
    char key;
} default_commands[] = {
    { "Continue", 'c' },
    { "Next", 'n' },
    { "Step", 's' },
    { "Machine step", 'm' },
    { "Return", 'r' },
    { "Signal", 'g' },
    { "Edit", 'e' },
    { "Restart", 't' },
    { "Exit", 'x' },
    { "Breakpoint", 'b' },
    { "Options", 'o' },
    { "Help", 'h' },
};

#define NCOMMANDS (sizeof default_commands / sizeof default_commands[0])

void dbxl_commands_init(xtk_pane *pane)
{
    char buf[NCOMMANDS][32];
    const char *lines[NCOMMANDS];

    for (size_t i = 0; i < NCOMMANDS; i++) {
        snprintf(buf[i], sizeof buf[i], "%-17sAlt-%c",
                 default_commands[i].name, default_commands[i].key);
        lines[i] = buf[i];
    }
    xtk_pane_set_lines(pane, lines, (int)NCOMMANDS);
}
