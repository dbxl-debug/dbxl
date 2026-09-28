#include "core/layout.h"

const xtk_rect dbxl_frame_geometry = { 33, 73, 957, 846 };

/*
 * Order matches xldb's window creation order, which also gives the initial
 * stacking order.  Titles are xldb's "no program" titles; the Storage
 * pane's title really is "Storage Pane".
 */
static const struct dbxl_window_def defs[DBXL_NWINDOWS] = {
    [DBXL_W_CALLERS]     = { "Callers", "Callers",
                             { 528, 85, 204, 282 }, { 528, 86, 204, 12 },
                             XTK_SB_NONE, DBXL_START_OPEN },
    [DBXL_W_FILES]       = { "Files", "Files",
                             { 653, 259, 277, 536 }, { 846, 4, 104, 12 },
                             XTK_SB_VERTICAL, DBXL_START_ICON },
    [DBXL_W_SUBPROGRAMS] = { "Subprograms", "Subprograms",
                             { 38, 136, 870, 209 }, { 528, 31, 134, 12 },
                             XTK_SB_VERTICAL, DBXL_START_ICON },
    [DBXL_W_BREAKPOINTS] = { "Breakpoints", "Breakpoints",
                             { 363, 200, 588, 160 }, { 672, 31, 134, 12 },
                             XTK_SB_NONE, DBXL_START_ICON },
    [DBXL_W_COMMANDS]    = { "Commands", "Commands",
                             { 744, 85, 204, 282 }, { 801, 85, 149, 12 },
                             XTK_SB_NONE, DBXL_START_OPEN },
    [DBXL_W_LOCALS]      = { "Locals", "Locals for [unknown]()",
                             { 3, 4, 516, 364 }, { 528, 4, 100, 12 },
                             XTK_SB_VERTICAL, DBXL_START_OPEN },
    [DBXL_W_SOURCE]      = { "Source", "Source",
                             { 3, 377, 947, 420 }, { 3, 377, 134, 12 },
                             XTK_SB_VERTICAL, DBXL_START_OPEN },
    [DBXL_W_GLOBALS]     = { "Globals", "Globals",
                             { 98, 111, 771, 517 }, { 634, 4, 100, 12 },
                             XTK_SB_VERTICAL, DBXL_START_ICON },
    [DBXL_W_MONITOR]     = { "Monitor", "Monitor",
                             { 50, 40, 771, 517 }, { 740, 4, 100, 12 },
                             XTK_SB_VERTICAL, DBXL_START_ICON },
    [DBXL_W_THREADS]     = { "Threads", "Threads",
                             { 3, 127, 784, 100 }, { 816, 31, 134, 12 },
                             XTK_SB_NONE, DBXL_START_ICON },
    [DBXL_W_DISASSEMBLY] = { "Disassembly", "unknown function name",
                             { 288, 377, 567, 440 }, { 528, 58, 134, 12 },
                             XTK_SB_NONE, DBXL_START_ICON },
    [DBXL_W_REGISTERS]   = { "Registers", "Registers",
                             { 741, 377, 207, 440 }, { 672, 58, 134, 12 },
                             XTK_SB_NONE, DBXL_START_ICON },
    [DBXL_W_STORAGE]     = { "Storage", "Storage Pane",
                             { 4, 377, 594, 440 }, { 816, 58, 134, 12 },
                             XTK_SB_NONE, DBXL_START_ICON },
    [DBXL_W_FORMATS]     = { "Formats", "Formats",
                             { 336, 141, 201, 460 }, { 336, 141, 201, 12 },
                             XTK_SB_NONE, DBXL_START_HIDDEN },
    [DBXL_W_MESSAGES]    = { "Messages", "",
                             { 3, 826, 947, 12 }, { 693, 3, 127, 12 },
                             XTK_SB_NONE, DBXL_START_HIDDEN },
    [DBXL_W_HELP]        = { "Help", "Help [exit][index][return]",
                             { 0, 0, 600, 750 }, { 773, 830, 180, 12 },
                             XTK_SB_VERTICAL | XTK_SB_HORIZONTAL,
                             DBXL_START_HIDDEN },
};

const struct dbxl_window_def *dbxl_window_def(enum dbxl_window_id id)
{
    return &defs[id];
}
