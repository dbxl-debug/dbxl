/*
 * The default window layout, taken from the geometry strings compiled into
 * xldb 1.2.1.0 and confirmed by the X protocol trace (DESIGN.md 5.3).
 */
#ifndef DBXL_LAYOUT_H
#define DBXL_LAYOUT_H

#include "xtk/xtk.h"

enum dbxl_start {
    DBXL_START_OPEN,      /* pane mapped at startup */
    DBXL_START_ICON,      /* chip mapped at startup */
    DBXL_START_HIDDEN     /* neither (Messages, Help, Formats) */
};

enum dbxl_window_id {
    DBXL_W_CALLERS,
    DBXL_W_FILES,
    DBXL_W_SUBPROGRAMS,
    DBXL_W_BREAKPOINTS,
    DBXL_W_COMMANDS,
    DBXL_W_LOCALS,
    DBXL_W_SOURCE,
    DBXL_W_GLOBALS,
    DBXL_W_MONITOR,
    DBXL_W_THREADS,
    DBXL_W_DISASSEMBLY,
    DBXL_W_REGISTERS,
    DBXL_W_STORAGE,
    DBXL_W_FORMATS,
    DBXL_W_MESSAGES,
    DBXL_W_HELP,
    DBXL_NWINDOWS
};

struct dbxl_window_def {
    const char *name;         /* resource name and chip label */
    const char *title;        /* title before a program is loaded */
    xtk_rect geom;            /* frame-relative, excluding the 2px border */
    xtk_rect icon;            /* chip geometry */
    int scrollbars;
    enum dbxl_start start;
};

extern const xtk_rect dbxl_frame_geometry;
const struct dbxl_window_def *dbxl_window_def(enum dbxl_window_id id);

#endif
