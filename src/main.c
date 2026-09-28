/*
 * dbxl - a Linux clone of IBM's AIX xldb debugger.
 */
#include <stdio.h>
#include <string.h>
#include <X11/Xutil.h>

#include "core/layout.h"
#include "opts.h"
#include "panes/commands.h"
#include "xtk/xtk.h"

static struct {
    xtk_pane *pane[DBXL_NWINDOWS];
    xtk_chip *chip[DBXL_NWINDOWS];
    Atom wm_protocols, wm_delete;
} ui;

static bool handle_event(const XEvent *ev, void *arg)
{
    (void)arg;
    if (ev->type == ClientMessage &&
        ev->xclient.message_type == ui.wm_protocols &&
        (Atom)ev->xclient.data.l[0] == ui.wm_delete) {
        xtk_loop_quit(0);
        return true;
    }
    for (int i = 0; i < DBXL_NWINDOWS; i++) {
        if (xtk_pane_handle_event(ui.pane[i], ev) ||
            xtk_chip_handle_event(ui.chip[i], ev))
            return true;
    }
    return false;
}

/*
 * The frame geometry in logical pixels.  Negative offsets count from the
 * right/bottom of the screen measured in logical pixels, so -geometry means
 * the same layout at every scale.  With no display yet (screen_w == 0) only
 * the size is resolved.
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

int main(int argc, char **argv)
{
    struct dbxl_opts o;
    char title[256];
    xtk_rect geom;
    int status, scr;

    if (dbxl_opts_parse(&o, argc, argv) < 0)
        return 255;
    geom = frame_geometry(o.geometry, 0, 0);
    if (!xtk_open(o.display, o.font, o.scale, geom.w + 4, geom.h + 4))
        return 1;
    scr = DefaultScreen(xtk_dpy());
    geom = frame_geometry(o.geometry,
                          DisplayWidth(xtk_dpy(), scr) / xtk_scale(),
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

    for (int i = 0; i < DBXL_NWINDOWS; i++) {
        const struct dbxl_window_def *d = dbxl_window_def(i);

        ui.pane[i] = xtk_pane_create(d->title, d->geom, d->scrollbars);
        ui.chip[i] = xtk_chip_create(d->name, d->icon);
        if (d->start == DBXL_START_OPEN)
            xtk_pane_map(ui.pane[i]);
        else if (d->start == DBXL_START_ICON)
            xtk_chip_map(ui.chip[i]);
    }
    dbxl_commands_init(ui.pane[DBXL_W_COMMANDS]);

    ui.wm_protocols = XInternAtom(xtk_dpy(), "WM_PROTOCOLS", False);
    ui.wm_delete = XInternAtom(xtk_dpy(), "WM_DELETE_WINDOW", False);
    xtk_loop_add_handler(handle_event, NULL);

    XMapWindow(xtk_dpy(), xtk_frame());
    status = xtk_loop_run();
    xtk_close();
    return status;
}
