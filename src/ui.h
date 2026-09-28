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

#endif
