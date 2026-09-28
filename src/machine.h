/*
 * The machine-level panes: Disassembly, Registers, Storage and Threads
 * (DESIGN.md 6, 6.1, 8; recon passes 5 and 14).
 */
#ifndef DBXL_MACHINE_H
#define DBXL_MACHINE_H

#include <stdint.h>

#include "backend/backend.h"
#include "xtk/xtk.h"

void dbxl_machine_init(void);
void dbxl_machine_set_backend(dbg_backend *b);
/* A stop: frame 0's pc, and the stop's reason for Threads. */
void dbxl_machine_stopped(uint64_t pc, enum dbg_stop_reason reason,
                          const char *signame);
/* A frame selected in Callers: its pc (a return address for callers). */
void dbxl_machine_select_frame(int level, uint64_t pc);
/* A function selected in Subprograms: its first instruction in cyan. */
void dbxl_machine_show_function(uint64_t addr);
/* The program is gone. */
void dbxl_machine_exited(void);
/* Breakpoints changed: redraw the Disassembly margin. */
void dbxl_machine_marks_changed(void);
/* "Storage view": show addr as Storage's top row. */
void dbxl_machine_storage_view(uint64_t addr);

void dbxl_machine_event(const dbg_event *ev);
void dbxl_machine_click(int window, const xtk_pane_event *e);

#endif
