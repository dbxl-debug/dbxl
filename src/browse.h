/*
 * Files and Subprograms (recon passes 3 and 14).
 */
#ifndef DBXL_BROWSE_H
#define DBXL_BROWSE_H

#include "backend/backend.h"
#include "xtk/xtk.h"

void dbxl_browse_event(const dbg_event *ev);
void dbxl_browse_click(int window, const xtk_pane_event *e);
/* The functions listed (with debug information unless Subprograms: All). */
int dbxl_browse_functions(const dbg_symbol **funcs);

#endif
