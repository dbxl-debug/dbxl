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

void dbxl_data_init(void);
void dbxl_data_set_backend(dbg_backend *b);
/* Fetch the values for a stop or a newly selected frame (func may be ""). */
void dbxl_data_refresh(const char *func, int level);
/* The program is gone: no locals. */
void dbxl_data_clear_locals(void);
/* A DBG_EV_VALUES event; takes ownership of v. */
void dbxl_data_values(dbg_values *v);
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
