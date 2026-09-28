/*
 * Files and Subprograms (recon passes 3 and 14).
 *
 * Files lists the program's source files, Subprograms its functions as
 * `name()`, both sorted.  Clicking a file shows it in Source from line 1
 * with that line in cyan; clicking a function shows it centred on its
 * first line (the `{`) in cyan, and its disassembly with the first
 * instruction in cyan.  Each click drops the other list's highlight.
 */
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "browse.h"
#include "core/options.h"
#include "core/layout.h"
#include "debugger.h"
#include "machine.h"
#include "ui.h"

static struct {
    dbg_symbol *files, *funcs;
    int nfiles, nfuncs;
} br;

static void set_list(int window, const dbg_symbol *syms, int n, bool funcs)
{
    char (*text)[300] = calloc((size_t)(n ? n : 1), sizeof *text);
    const char **lines = calloc((size_t)(n ? n : 1), sizeof *lines);

    for (int i = 0; i < n; i++) {
        if (funcs && dbxl_opt.subprograms_all)
            /* Subprograms: All -- `<address>:   name()` (recon pass 15). */
            snprintf(text[i], sizeof text[i], "%016" PRIx64 ":   %s()",
                     syms[i].addr, syms[i].name);
        else if (funcs)
            snprintf(text[i], sizeof text[i], "%s()", syms[i].name);
        else
            snprintf(text[i], sizeof text[i], "%s", syms[i].file);
        lines[i] = text[i];
    }
    xtk_pane_set_lines(dbxl_ui_pane(window), lines, n);
    free(lines);
    free(text);
}

void dbxl_browse_event(const dbg_event *ev)
{
    if (ev->type != DBG_EV_SYMBOLS)
        return;
    free(br.files);
    free(br.funcs);
    br.files = calloc((size_t)ev->nfiles + 1, sizeof *br.files);
    br.funcs = calloc((size_t)ev->nfuncs + 1, sizeof *br.funcs);
    memcpy(br.files, ev->files, (size_t)ev->nfiles * sizeof *br.files);
    memcpy(br.funcs, ev->funcs, (size_t)ev->nfuncs * sizeof *br.funcs);
    br.nfiles = ev->nfiles;
    br.nfuncs = ev->nfuncs;
    set_list(DBXL_W_FILES, br.files, br.nfiles, false);
    set_list(DBXL_W_SUBPROGRAMS, br.funcs, br.nfuncs, true);
}

void dbxl_browse_click(int window, const xtk_pane_event *e)
{
    if (e->line < 0 || (window == DBXL_W_FILES && e->line >= br.nfiles) ||
        (window == DBXL_W_SUBPROGRAMS && e->line >= br.nfuncs))
        return;
    /* xldb drops the Callers highlight here too (recon pass 14). */
    xtk_pane_set_selected(dbxl_ui_pane(DBXL_W_CALLERS), -1);
    if (window == DBXL_W_FILES && e->line >= 0 && e->line < br.nfiles) {
        const dbg_symbol *f = &br.files[e->line];

        xtk_pane_set_selected(dbxl_ui_pane(DBXL_W_FILES), e->line);
        xtk_pane_set_selected(dbxl_ui_pane(DBXL_W_SUBPROGRAMS), -1);
        dbxl_debug_show_location(f->file, f->fullname, 1, true);
    } else if (window == DBXL_W_SUBPROGRAMS && e->line >= 0 &&
               e->line < br.nfuncs) {
        const dbg_symbol *f = &br.funcs[e->line];

        xtk_pane_set_selected(dbxl_ui_pane(DBXL_W_SUBPROGRAMS), e->line);
        xtk_pane_set_selected(dbxl_ui_pane(DBXL_W_FILES), -1);
        dbxl_debug_show_location(f->file, f->fullname, f->line, false);
        dbxl_machine_show_function(f->addr);
    }
}

int dbxl_browse_functions(const dbg_symbol **funcs)
{
    *funcs = br.funcs;
    return br.nfuncs;
}
