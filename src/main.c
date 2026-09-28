/*
 * dbxl - a Linux clone of IBM's AIX xldb debugger.
 */
#include <stdio.h>
#include <string.h>
#include <X11/Xutil.h>

#include "core/layout.h"
#include "core/options.h"
#include "core/resources.h"
#include "debugger.h"
#include "opts.h"
#include "ui.h"
#include "xtk/xtk.h"

static Atom wm_protocols, wm_delete;

static bool close_event(const XEvent *ev, void *arg)
{
    (void)arg;
    if (ev->type == ClientMessage &&
        ev->xclient.message_type == wm_protocols &&
        (Atom)ev->xclient.data.l[0] == wm_delete) {
        xtk_loop_quit(0);
        return true;
    }
    return false;
}

/*
 * The frame geometry in logical pixels.  Negative offsets count from the
 * right/bottom of the screen measured in logical pixels, so -geometry means
 * the same layout at every scale.  With no screen size yet (0) only the
 * size is resolved.
 */
static xtk_rect frame_geometry(const char *spec, int screen_w, int screen_h)
{
    xtk_rect g = dbxl_frame_geometry;
    int gx, gy;
    unsigned gw, gh;
    int mask, outer = 4;          /* 2px border on each side */

    if (!spec)
        return g;
    mask = XParseGeometry(spec, &gx, &gy, &gw, &gh);
    if (mask & WidthValue)
        g.w = (int)gw;
    if (mask & HeightValue)
        g.h = (int)gh;
    if (mask & XValue)
        g.x = (mask & XNegative) ? screen_w + gx - g.w - outer : gx;
    if (mask & YValue)
        g.y = (mask & YNegative) ? screen_h + gy - g.h - outer : gy;
    return g;
}

/* Colour resources and the roles they set (DESIGN.md 5.1). */
static const struct {
    const char *resource;
    enum xtk_color role;
} color_resources[] = {
    { "background", XTK_BG },
    { "foreground", XTK_FG },
    { "titleBackground", XTK_TITLE_BG },
    { "titleForeground", XTK_TITLE_FG },
    { "borderActive", XTK_BORDER_ACTIVE },
    { "borderIdle", XTK_BORDER_IDLE },
    { "dialogBackground", XTK_DIALOG_BG },
    { "dialogForeground", XTK_DIALOG_FG },
    { "menuInfoForeground", XTK_MENUINFO_FG },
    { "markBackground", XTK_MARK_BG },
    { "markForeground", XTK_MARK_FG },
    { "cursorForeground", XTK_CURSOR },
    { "mouseBody", XTK_POINTER_BODY },
    { "mouseOutline", XTK_POINTER_OUTLINE },
    { "scrollButtonIdle", XTK_THUMB },
};

int main(int argc, char **argv)
{
    struct dbxl_opts o;
    struct dbxl_ui_config ucfg;
    struct dbxl_debug_config dcfg;
    xtk_config cfg;
    char title[256];
    const char *geometry;
    xtk_rect geom;
    int status, scr;

    if (dbxl_opts_parse(&o, argc, argv) < 0)
        return 255;
    if (!xtk_open(o.display))
        return 1;
    dbxl_res_init(xtk_dpy(), o.name ? o.name : dbxl_res_default_name(argv[0]));
    dbxl_options_defaults();

    memset(&cfg, 0, sizeof cfg);
    cfg.font = o.font ? o.font : dbxl_res_str("font");
    if (!cfg.font)
        cfg.font = "8x13";
    cfg.scale = o.scale >= 0 ? o.scale : dbxl_res_int("scale", 1);
    if (o.scheme >= 0)
        cfg.scheme = (enum xtk_scheme)o.scheme;
    else if (dbxl_res_bool("blackOnWhite", false))
        cfg.scheme = XTK_SCHEME_BW;
    else if (dbxl_res_bool("whiteOnBlack", false))
        cfg.scheme = XTK_SCHEME_WB;
    for (size_t i = 0; i < sizeof color_resources / sizeof color_resources[0]; i++)
        cfg.color[color_resources[i].role] =
            dbxl_res_str(color_resources[i].resource);
    if (o.bg)
        cfg.color[XTK_BG] = o.bg;
    if (o.fg)
        cfg.color[XTK_FG] = o.fg;

    geometry = o.geometry ? o.geometry : dbxl_res_str("geometry");
    geom = frame_geometry(geometry, 0, 0);
    cfg.fit_w = geom.w + 4;
    cfg.fit_h = geom.h + 4;
    if (!xtk_setup(&cfg))
        return 1;
    scr = DefaultScreen(xtk_dpy());
    geom = frame_geometry(geometry, DisplayWidth(xtk_dpy(), scr) / xtk_scale(),
                          DisplayHeight(xtk_dpy(), scr) / xtk_scale());

    if (o.title) {
        snprintf(title, sizeof title, "%s", o.title);
    } else if (o.program) {
        const char *base = strrchr(o.program, '/');
        snprintf(title, sizeof title, "dbxl %s", base ? base + 1 : o.program);
    } else {
        snprintf(title, sizeof title, "dbxl");
    }
    xtk_frame_create(geom, title, "dbxl", argc, argv);

    dbxl_opt.autoraise = dbxl_res_bool("autoRaise", true);
    ucfg.tag_windows = dbxl_res_bool("tagWindows", false);
    ucfg.confirm_exit = dbxl_res_bool("confirmExit", true);
    ucfg.command_list = dbxl_res_str("commandList");
    dbxl_ui_init(&ucfg);

    wm_protocols = XInternAtom(xtk_dpy(), "WM_PROTOCOLS", False);
    wm_delete = XInternAtom(xtk_dpy(), "WM_DELETE_WINDOW", False);
    xtk_loop_add_handler(close_event, NULL);

    XMapWindow(xtk_dpy(), xtk_frame());

    memset(&dcfg, 0, sizeof dcfg);
    dcfg.program = o.program;
    dcfg.argc = o.prog_argc;
    dcfg.argv = o.prog_argv;
    dcfg.run_to = dbxl_res_str("runTo");
    dcfg.edit = o.edit ? o.edit : dbxl_res_str("edit");
    dcfg.include = o.include;
    dcfg.ninclude = o.ninclude;
    dbxl_debug_init(&dcfg);

    status = xtk_loop_run();
    dbxl_debug_shutdown();
    dbxl_res_free();
    xtk_close();
    return status;
}
