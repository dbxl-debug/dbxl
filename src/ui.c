/*
 * The user interface controller (recon passes 1-11).
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "core/commandlist.h"
#include "core/layout.h"
#include "core/options.h"
#include "core/resources.h"
#include "browse.h"
#include "data.h"
#include "debugger.h"
#include "help.h"
#include "machine.h"
#include "ui.h"
#include "xtk/xtk.h"

/* Window tags for tagWindows (recon pass 11); NULL = no tag. */
static const char *const tags[DBXL_NWINDOWS] = {
    [DBXL_W_CALLERS] = "Ca",     [DBXL_W_FILES] = "F",
    [DBXL_W_SUBPROGRAMS] = "Su", [DBXL_W_BREAKPOINTS] = "B",
    [DBXL_W_COMMANDS] = "Co",    [DBXL_W_LOCALS] = "L",
    [DBXL_W_SOURCE] = "So",      [DBXL_W_GLOBALS] = "G",
    [DBXL_W_MONITOR] = "M",      [DBXL_W_THREADS] = "Th",
    [DBXL_W_STORAGE] = "St",     [DBXL_W_HELP] = "H",
};

struct window {
    xtk_pane *pane;
    xtk_chip *chip;
    bool maximized;
    xtk_rect restore;            /* geometry before Maximize */
};

static struct {
    struct window w[DBXL_NWINDOWS];
    struct dbxl_command cmd[DBXL_MAX_COMMANDS];
    int ncmd;
    bool confirm_exit;
    int wc_default;              /* sticky default item of Window Control */
    bool special_layout;         /* the specialLayout resource */
    bool message_shown;
} ui;

static int window_of_pane(const xtk_pane *p)
{
    for (int i = 0; i < DBXL_NWINDOWS; i++)
        if (ui.w[i].pane == p)
            return i;
    return -1;
}

/* ---- Messages -------------------------------------------------------- */

void dbxl_ui_message(const char *text)
{
    xtk_pane *m = ui.w[DBXL_W_MESSAGES].pane;

    xtk_pane_set_title(m, text);
    xtk_pane_map(m);
    ui.message_shown = true;
}

/* xldb hides the message at the next button press in a pane. */
static void clear_message(void)
{
    if (ui.message_shown) {
        xtk_pane_unmap(ui.w[DBXL_W_MESSAGES].pane);
        ui.message_shown = false;
    }
}

/* ---- opening and minimizing ------------------------------------------ */

static void open_window(int i)
{
    xtk_chip_unmap(ui.w[i].chip);
    xtk_pane_map(ui.w[i].pane);
}

xtk_pane *dbxl_ui_pane(int window)
{
    return ui.w[window].pane;
}

void dbxl_ui_show_window(int window)
{
    if (xtk_pane_mapped(ui.w[window].pane))
        xtk_pane_raise(ui.w[window].pane);
    else
        open_window(window);
}

static void minimize_window(int i)
{
    xtk_pane_unmap(ui.w[i].pane);
    xtk_chip_map(ui.w[i].chip);
}

static void chip_clicked(xtk_chip *c, int button, void *arg)
{
    (void)c;
    if (button == Button1)
        open_window((int)(intptr_t)arg);
}

/* ---- Save Window ----------------------------------------------------- */

static void save_window_done(int button, const char *text, void *arg)
{
    xtk_pane *p = arg;
    FILE *f;

    if (button != 0 || !*text)
        return;
    f = fopen(text, "w");
    if (!f) {
        char msg[300];
        snprintf(msg, sizeof msg, "Unable to open %s", text);
        dbxl_ui_message(msg);
        return;
    }
    /* xldb writes each line with a trailing space (recon pass 6). */
    for (int i = 0; i < xtk_pane_nlines(p); i++)
        fprintf(f, "%s \n", xtk_pane_line(p, i));
    fclose(f);
}

/* ---- Window Control menu --------------------------------------------- */

enum { WC_RESTORE, WC_MOVE, WC_SIZE, WC_MINIMIZE, WC_MAXIMIZE, WC_LOWER,
       WC_HSCROLL, WC_VSCROLL, WC_SAVE, WC_NITEMS };

static const char *const wc_items[WC_NITEMS] = {
    "Restore", "Move", "Size", "Minimize", "Maximize", "Lower",
    "Horizontal scroll bars", "Vertical scroll bars", "Save Window",
};

static void wc_selected(xtk_menu *m, int item, int button, void *arg)
{
    int i = (int)(intptr_t)arg;
    struct window *w = &ui.w[i];
    int ax, ay;

    (void)button;
    xtk_menu_anchor(m, &ax, &ay);
    ui.wc_default = item;
    xtk_menu_close(m, true);

    switch (item) {
    case WC_RESTORE:
        if (w->maximized) {
            xtk_pane_set_geometry(w->pane, w->restore);
            w->maximized = false;
        }
        break;
    case WC_MOVE:
        xtk_pane_begin_move(w->pane);
        break;
    case WC_SIZE:
        xtk_pane_begin_size(w->pane);
        break;
    case WC_MINIMIZE:
        minimize_window(i);
        break;
    case WC_MAXIMIZE:
        if (!w->maximized) {
            w->restore = xtk_pane_geometry(w->pane);
            w->maximized = true;
        }
        /* The frame less a 2px margin: 2,2 949x838 (recon pass 11). */
        xtk_pane_set_geometry(w->pane, (xtk_rect){ 2, 2, xtk_frame_w() - 8,
                                                   xtk_frame_h() - 8 });
        xtk_pane_raise(w->pane);
        break;
    case WC_LOWER:
        xtk_pane_lower(w->pane);
        break;
    case WC_HSCROLL:
        xtk_pane_set_scrollbars(w->pane,
                                xtk_pane_scrollbars(w->pane) ^ XTK_SB_HORIZONTAL);
        break;
    case WC_VSCROLL:
        xtk_pane_set_scrollbars(w->pane,
                                xtk_pane_scrollbars(w->pane) ^ XTK_SB_VERTICAL);
        break;
    case WC_SAVE:
        /* xldb's title ends with a space, which shifts it left. */
        xtk_dialog_prompt("Enter the target file name ", "", "proceed",
                          "cancel", ax, ay, save_window_done, w->pane);
        break;
    }
}

static void window_control(int i, int fx, int fy)
{
    xtk_rect g = xtk_pane_geometry(ui.w[i].pane);
    xtk_menu_spec spec = { "Window Control", wc_items, WC_NITEMS, 0,
                           ui.wc_default };

    xtk_menu_open_at(&spec, g.x, g.y, fx, fy, wc_selected,
                     (void *)(intptr_t)i);
}

/* ---- Options menu ---------------------------------------------------- */

static void options_menu(int ax, int ay);

static void registers_selected(xtk_menu *m, int item, int button, void *arg)
{
    char buf[64];

    (void)button;
    (void)arg;
    dbxl_options_register_cycle(item);
    dbxl_options_register_item(item, buf, sizeof buf);
    xtk_menu_set_item(m, item, buf);
}

static void register_control(int ax, int ay)
{
    char text[REG_NITEMS][64];
    const char *items[REG_NITEMS];
    xtk_menu_spec spec = { "Display controls", items, REG_NITEMS, 0, 0 };

    for (int k = 0; k < REG_NITEMS; k++) {
        dbxl_options_register_item(k, text[k], sizeof text[k]);
        items[k] = text[k];
    }
    xtk_menu_open_anchored(&spec, ax, ay, registers_selected, NULL);
}

/*
 * Save layout (recon pass 15): the resources that recreate this layout,
 * named for this program (dbxl, or -name), in xldb's order.
 */
static const char *sb_name(int flags)
{
    switch (flags) {
    case XTK_SB_VERTICAL: return "vertical";
    case XTK_SB_HORIZONTAL: return "horizontal";
    case XTK_SB_VERTICAL | XTK_SB_HORIZONTAL: return "both";
    default: return "none";
    }
}

static void put_geom(FILE *f, const char *name, const char *res, xtk_rect g)
{
    fprintf(f, "%s.%s:%dx%d+%d+%d\n", dbxl_res_name(), res, g.w, g.h, g.x, g.y);
    (void)name;
}

static int save_layout(const char *file)
{
    static const char *const order[] = {
        "Callers", "Files", "Subprograms", "Breakpoints", "Commands", "Source",
        "Locals", "Globals", "Monitor", "Formats", "Messages", "Help",
        "Disassembly", "Registers", "Storage", "Threads",
    };
    const char *n = dbxl_res_name();
    FILE *f = fopen(file, "w");
    xtk_rect fr = xtk_frame_geometry();

    if (!f)
        return -1;
    fprintf(f, "%s.AutoRaise:%s\n", n, dbxl_opt.autoraise ? "on" : "off");
    fprintf(f, "%s.FunctionList:%s\n", n,
            dbxl_opt.subprograms_all ? "All" : "With -g  only");
    fprintf(f, "%s.specialLayout:true\n", n);
    fprintf(f, "%s.geometry:%dx%d+%d+%d\n", n, fr.w, fr.h, fr.x, fr.y);
    for (size_t k = 0; k < sizeof order / sizeof order[0]; k++) {
        char res[64];

        if (strcmp(order[k], "Formats") == 0) {
            xtk_rect g = dbxl_data_formats_geometry();
            put_geom(f, order[k], "FormatsGeometry", g);
            put_geom(f, order[k], "FormatsIconGeometry",
                     (xtk_rect){ g.x, g.y, g.w, 12 });
            continue;
        }
        for (int i = 0; i < DBXL_NWINDOWS; i++) {
            const struct dbxl_window_def *d = dbxl_window_def(i);
            struct window *w = &ui.w[i];

            if (strcmp(d->name, order[k]) != 0)
                continue;
            snprintf(res, sizeof res, "%sGeometry", d->name);
            put_geom(f, d->name, res, xtk_pane_geometry(w->pane));
            snprintf(res, sizeof res, "%sIconGeometry", d->name);
            put_geom(f, d->name, res, xtk_chip_geometry(w->chip));
            if (i == DBXL_W_MESSAGES)
                break;
            fprintf(f, "%s.%sIconStartup:%s\n", n, d->name,
                    xtk_chip_mapped(w->chip) ? "true" : "false");
            fprintf(f, "%s.%sScrollbars:%s\n", n, d->name,
                    sb_name(xtk_pane_scrollbars(w->pane)));
        }
    }
    return fclose(f);
}

static void save_layout_done(int button, const char *text, void *arg)
{
    char msg[600];

    (void)arg;
    if (button != 0)
        return;
    if (!text[0] || save_layout(text) != 0)
        snprintf(msg, sizeof msg, "Unable to save window layout in file %s.", text);
    else
        snprintf(msg, sizeof msg, "Window layout saved in file %s.", text);
    dbxl_ui_message(msg);
}

static void save_breakpoints_done(int button, const char *text, void *arg)
{
    (void)arg;
    if (button == 0 && text[0])
        dbxl_debug_save_breakpoints(text);
}

static void load_breakpoints_done(int button, const char *text, void *arg)
{
    (void)arg;
    if (button == 0 && text[0])
        dbxl_debug_load_breakpoints(text);
}

static void source_path_done(int button, const char *text, void *arg)
{
    (void)arg;
    if (button == 0)
        snprintf(dbxl_opt.source_path, sizeof dbxl_opt.source_path, "%s", text);
}

static void options_selected(xtk_menu *m, int item, int button, void *arg)
{
    char buf[64];
    int ax, ay, px, py;

    (void)arg;
    xtk_menu_anchor(m, &ax, &ay);
    /* The dialogs warp back to the chosen item, not to Options (recon
     * pass 16). */
    xtk_pointer_frame(&px, &py);
    switch (item) {
    case OPT_REGISTER_CONTROL:
        /* The submenu replaces the Options menu at the same anchor. */
        xtk_menu_close(m, false);
        register_control(ax, ay);
        return;
    case OPT_SOURCE_PATH:
        xtk_menu_close(m, true);
        xtk_dialog_prompt("Edit source search path", dbxl_opt.source_path,
                          "proceed", "cancel", px, py, source_path_done, NULL);
        return;
    case OPT_SAVE_LAYOUT:
        xtk_menu_close(m, true);
        xtk_dialog_prompt("layout file name", "", "proceed", "cancel", px, py,
                          save_layout_done, NULL);
        return;
    case OPT_SAVE_BREAKPOINTS:
    case OPT_LOAD_BREAKPOINTS:
        xtk_menu_close(m, true);
        /* xldb titles both dialogs this way (recon pass 15). */
        xtk_dialog_prompt("save breakpoints file name",
                          dbxl_debug_breakpoints_file(), "proceed", "cancel",
                          px, py, item == OPT_SAVE_BREAKPOINTS
                                      ? save_breakpoints_done
                                      : load_breakpoints_done, NULL);
        return;
    }
    if (dbxl_options_cycle(item, button != Button3)) {
        if (item == OPT_AUTORAISE)
            xtk_pane_set_autoraise(dbxl_opt.autoraise);
        else if (item == OPT_SUBPROGRAMS || item == OPT_LOCAL_VARIABLES)
            dbxl_debug_options_changed(item);
        else if (item == OPT_BREAK_ALL && dbxl_opt.break_all)
            dbxl_debug_break_all();
        dbxl_options_item(item, buf, sizeof buf);
        xtk_menu_set_item(m, item, buf);   /* the menu stays open */
    }
}

static void options_menu(int ax, int ay)
{
    char text[OPT_NITEMS][64];
    const char *items[OPT_NITEMS];
    xtk_menu_spec spec = { "Options", items, OPT_NITEMS,
                           DBXL_OPTIONS_MENU_WIDTH, 0 };

    for (int k = 0; k < OPT_NITEMS; k++) {
        dbxl_options_item(k, text[k], sizeof text[k]);
        items[k] = text[k];
    }
    xtk_menu_open_anchored(&spec, ax, ay, options_selected, NULL);
}

/* ---- Commands -------------------------------------------------------- */

static void exit_done(int button, const char *text, void *arg)
{
    (void)text;
    (void)arg;
    if (button == 0)
        xtk_loop_quit(0);
}

static void breakpoint_done(int button, const char *text, void *arg)
{
    (void)arg;
    if (button == 0)
        dbxl_debug_break_function(text);
}

static void run_command(const struct dbxl_command *c, int ax, int ay)
{
    switch (c->action) {
    case ACT_EXIT:
        if (ui.confirm_exit)
            xtk_dialog_confirm("Exit from dbxl?", "yes", "no", ax, ay,
                               exit_done, NULL);
        else
            xtk_loop_quit(0);
        break;
    case ACT_OPTIONS:
        options_menu(ax, ay);
        break;
    case ACT_BREAKPOINT:
        xtk_dialog_prompt("Enter function name", "", "proceed", "cancel",
                          ax, ay, breakpoint_done, NULL);
        break;
    case ACT_HELP:
        dbxl_help_open();
        break;
    case ACT_CONTINUE:
    case ACT_NEXT:
    case ACT_STEP:
    case ACT_MACHINE_STEP:
    case ACT_RETURN:
    case ACT_RESTART:
    case ACT_EDIT:
        dbxl_debug_command(c->action);
        break;
    default:
        /* Signal and subprogram calls: milestone 6. */
        break;
    }
}

static void accelerator(char key, int ax, int ay)
{
    for (int k = 0; k < ui.ncmd; k++)
        if (ui.cmd[k].key == key) {
            run_command(&ui.cmd[k], ax, ay);
            return;
        }
}

/* ---- the input strip: :n, /x, \\x, ?x (xldb's help, 3.1) -------------- */

static char last_search[256];

static void strip_command(xtk_pane *p, const char *text)
{
    int line, col, fl, fc, dir;
    const char *s;

    if (!text || !text[0])
        return;
    xtk_pane_cursor(p, &line, &col);
    if (text[0] == ':') {
        if (text[1] >= '0' && text[1] <= '9')
            xtk_pane_goto(p, atoi(text + 1) - 1, 0);
        return;
    }
    dir = text[0] == '/' ? 1 : -1;
    s = text + 1;
    if (*s)
        snprintf(last_search, sizeof last_search, "%s", s);
    else
        s = last_search;
    if (xtk_pane_search(p, s, dir, !dbxl_opt.case_sensitive, line, col, &fl, &fc))
        xtk_pane_goto(p, fl, fc);
}

/* ---- pane input -------------------------------------------------------- */

static void pane_event(xtk_pane *p, const xtk_pane_event *e, void *arg)
{
    int i = window_of_pane(p);
    char msg[128];

    (void)arg;
    switch (e->type) {
    case XTK_PANE_TITLE_CLICK:
        clear_message();
        if (e->button != Button1)
            break;
        /* Help's title has [exit][index][return] (recon pass 15). */
        if (i == DBXL_W_HELP &&
            dbxl_help_title_click(e->x, xtk_pane_geometry(p).w))
            break;
        window_control(i, e->fx, e->fy);
        break;
    case XTK_PANE_CLICK:
        clear_message();
        if (e->button != Button1)
            break;
        if (i == DBXL_W_COMMANDS && e->row >= 0 && e->row < ui.ncmd) {
            run_command(&ui.cmd[e->row], e->fx, e->fy);
        } else if (i == DBXL_W_SOURCE && dbxl_debug_active()) {
            dbxl_debug_source_click(e->line + 1);
        } else if (i == DBXL_W_SOURCE) {
            /* No program: xldb's message for a line it can't break on. */
            snprintf(msg, sizeof msg, "Cannot breakpoint line %d of `'",
                     e->line + 1);
            dbxl_ui_message(msg);
        } else if (dbxl_debug_active() &&
                   (i == DBXL_W_LOCALS || i == DBXL_W_GLOBALS ||
                    i == DBXL_W_MONITOR)) {
            dbxl_data_click(i, e);
        } else if (i == DBXL_W_GLOBALS || i == DBXL_W_MONITOR) {
            dbxl_ui_message("Click on a data object.");
        } else if (i == DBXL_W_HELP) {
            dbxl_help_click(e->line);
        } else if (i == DBXL_W_CALLERS && dbxl_debug_active()) {
            dbxl_debug_select_frame(e->line);
        } else if (i == DBXL_W_BREAKPOINTS && dbxl_debug_active()) {
            dbxl_debug_breakpoints_click(e);
        } else if ((i == DBXL_W_DISASSEMBLY || i == DBXL_W_REGISTERS ||
                    i == DBXL_W_STORAGE) && dbxl_debug_active()) {
            dbxl_machine_click(i, e);
        } else if ((i == DBXL_W_FILES || i == DBXL_W_SUBPROGRAMS) &&
                   dbxl_debug_active()) {
            dbxl_browse_click(i, e);
        }
        break;
    case XTK_PANE_KEY:
        if ((e->state & Mod1Mask) && e->text && e->text[0])
            accelerator(e->text[0], e->fx, e->fy);
        break;
    case XTK_PANE_STRIP_DONE:
        strip_command(p, e->text);
        break;
    }
}

/* ---- setup ----------------------------------------------------------- */

static int parse_scrollbars(const char *s, int dflt)
{
    if (!s)
        return dflt;
    if (!strcmp(s, "vertical")) return XTK_SB_VERTICAL;
    if (!strcmp(s, "horizontal")) return XTK_SB_HORIZONTAL;
    if (!strcmp(s, "both")) return XTK_SB_VERTICAL | XTK_SB_HORIZONTAL;
    if (!strcmp(s, "none")) return XTK_SB_NONE;
    return dflt;
}

/* A per-window geometry resource "WxH+X+Y" (frame coordinates). */
static xtk_rect parse_geometry(const char *s, xtk_rect dflt)
{
    int x, y;
    unsigned w, h;
    int mask;

    if (!s)
        return dflt;
    mask = XParseGeometry(s, &x, &y, &w, &h);
    if (mask & WidthValue) dflt.w = (int)w;
    if (mask & HeightValue) dflt.h = (int)h;
    if (mask & XValue) dflt.x = x;
    if (mask & YValue) dflt.y = y;
    return dflt;
}

/* A per-window layout resource, only with specialLayout. */
static const char *layout_res(const char *resource)
{
    return ui.special_layout ? dbxl_res_str(resource) : NULL;
}

static bool global_event(const XEvent *ev, void *arg)
{
    (void)arg;
    if (xtk_gesture_handle_event(ev) || xtk_dialog_handle_event(ev) ||
        xtk_menu_handle_event(ev) || xtk_field_handle_event(ev) ||
        dbxl_data_handle_event(ev))
        return true;
    for (int i = 0; i < DBXL_NWINDOWS; i++)
        if (xtk_pane_handle_event(ui.w[i].pane, ev) ||
            xtk_chip_handle_event(ui.w[i].chip, ev))
            return true;
    return false;
}

void dbxl_ui_init(const struct dbxl_ui_config *cfg)
{
    const char *lines[DBXL_MAX_COMMANDS];
    char text[DBXL_MAX_COMMANDS][128];

    ui.confirm_exit = cfg->confirm_exit;
    ui.wc_default = WC_MINIMIZE;
    xtk_pane_set_autoraise(dbxl_opt.autoraise);

    /* xldb uses the per-window layout resources only with specialLayout
     * true (its help, "Configuring xldb using .Xdefaults"). */
    ui.special_layout = dbxl_res_bool("specialLayout", false);
    for (int i = 0; i < DBXL_NWINDOWS; i++) {
        const struct dbxl_window_def *d = dbxl_window_def(i);
        struct window *w = &ui.w[i];
        char res[64];
        xtk_rect geom, icon;
        int sb;
        enum dbxl_start start = d->start;

        snprintf(res, sizeof res, "%sGeometry", d->name);
        geom = parse_geometry(layout_res(res), d->geom);
        snprintf(res, sizeof res, "%sIconGeometry", d->name);
        icon = parse_geometry(layout_res(res), d->icon);
        icon.h = d->icon.h;                  /* chips are one title high */
        snprintf(res, sizeof res, "%sScrollbars", d->name);
        sb = parse_scrollbars(layout_res(res), d->scrollbars);
        snprintf(res, sizeof res, "%sIconStartup", d->name);
        /* Help, Messages and Formats start hidden whatever IconStartup says. */
        if (start != DBXL_START_HIDDEN) {
            const char *v = layout_res(res);
            if (v)
                start = dbxl_res_bool(res, false) ? DBXL_START_ICON
                                                  : DBXL_START_OPEN;
        }

        w->pane = xtk_pane_create(d->title, geom, sb);
        w->chip = xtk_chip_create(d->name, icon);
        xtk_pane_set_handler(w->pane, pane_event, NULL);
        xtk_chip_set_handler(w->chip, chip_clicked, (void *)(intptr_t)i);
        if (cfg->tag_windows && tags[i])
            xtk_pane_set_tag(w->pane, tags[i]);
        if (start == DBXL_START_OPEN)
            xtk_pane_map(w->pane);
        else if (start == DBXL_START_ICON)
            xtk_chip_map(w->chip);
    }

    ui.ncmd = dbxl_commandlist_parse(cfg->command_list, ui.cmd,
                                     DBXL_MAX_COMMANDS);
    for (int k = 0; k < ui.ncmd; k++) {
        dbxl_command_line(&ui.cmd[k], text[k], sizeof text[k]);
        lines[k] = text[k];
    }
    xtk_pane_set_lines(ui.w[DBXL_W_COMMANDS].pane, lines, ui.ncmd);

    /* Static content xldb shows before any data (recon pass 11). */
    {
        static const char *const monitor[] = { "----- Globals -----" };
        static const char *const threads[] = {
            "-------Label-------  -Thread-  -Mode-  --Wait--  --wchan-  "
            "----SIGNAL---  --State-",
            "    All Threads",
        };
        xtk_pane_set_lines(ui.w[DBXL_W_MONITOR].pane, monitor, 1);
        xtk_pane_set_lines(ui.w[DBXL_W_THREADS].pane, threads, 2);
    }
    /* "Formats" is xldb's name for the variable menu (recon pass 15). */
    dbxl_data_init(parse_geometry(layout_res("FormatsGeometry"),
                                  DBXL_FORMATS_GEOMETRY));
    dbxl_machine_init();
    xtk_loop_add_handler(global_event, NULL);
}
