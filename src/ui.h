/*
 * The user interface controller: owns the windows and chips and connects
 * toolkit events to xldb's behaviour (Window Control, Commands, Options,
 * Messages).
 */
#ifndef DBXL_UI_H
#define DBXL_UI_H

#include <stdbool.h>

struct dbxl_ui_config {
    bool tag_windows;
    bool confirm_exit;
    const char *command_list;     /* NULL = defaults */
};

void dbxl_ui_init(const struct dbxl_ui_config *cfg);
void dbxl_ui_message(const char *text);
/* The pane of a window (enum in core/layout.h). */
struct xtk_pane *dbxl_ui_pane(int window);
/* Open a window if it is minimized, else raise it. */
void dbxl_ui_show_window(int window);

#endif
