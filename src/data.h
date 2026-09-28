/*
 * The data panes (Locals, Globals, Monitor) and the variable menu
 * (DESIGN.md 8-9, recon pass 13).
 */
#ifndef DBXL_DATA_H
#define DBXL_DATA_H

#include <stdbool.h>
#include <X11/Xlib.h>

#include "backend/backend.h"
#include "xtk/xtk.h"

/* formats: the Formats window's (the variable menu's) geometry. */
void dbxl_data_init(xtk_rect formats);
/* Its default geometry (recon pass 13). */
#define DBXL_FORMATS_GEOMETRY ((xtk_rect){ 336, 141, 201, 460 })
void dbxl_data_set_backend(dbg_backend *b);
/* Fetch the values for a stop or a newly selected frame (func may be ""). */
void dbxl_data_refresh(const char *func, int level);
/* The program is gone: no locals. */
void dbxl_data_clear_locals(void);
/* The program ended: Locals, Globals and Monitor empty (recon pass 17). */
void dbxl_data_clear_all(void);
/* The address chosen with Function parameter, if any. */
bool dbxl_data_function_parameter(uint64_t *addr);
/* The target's pointer size, 0 before any values arrived. */
int dbxl_data_ptrsize(void);
/* The Formats window's current geometry (for Save layout). */
xtk_rect dbxl_data_formats_geometry(void);
/* A DBG_EV_VALUES event; takes ownership of v. */
void dbxl_data_values(dbg_values *v);
/* A DBG_EV_TYPES event: the program's types, for Cast. */
void dbxl_data_types(const char *const *names, int n);
/* A DBG_EV_ASSIGNED event. */
void dbxl_data_assigned(void);
/* A left click in Locals, Globals or Monitor. */
void dbxl_data_click(int window, const xtk_pane_event *e);
/* Open the variable menu for a rendered element (Registers uses it). */
struct dbxl_span;
void dbxl_data_open_menu(const struct dbxl_span *s, int fx, int fy);
/* The menu pane's events, and clicks elsewhere that close it. */
bool dbxl_data_handle_event(const XEvent *ev);

#endif
