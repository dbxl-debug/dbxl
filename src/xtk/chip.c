/*
 * Chips: the title-only windows that stand in for minimized panes.  Drawn
 * like a title bar: a MediumBlue fill with the label centred in white.
 */
#include <stdlib.h>
#include <string.h>

#include "xtk/xtk.h"

struct xtk_chip {
    Window win;
    xtk_surface surf;
    char *label;
    int w, h;
};

static void draw(xtk_chip *c);

xtk_chip *xtk_chip_create(const char *label, xtk_rect geom)
{
    const xtk_metrics *m = xtk_metrics_get();
    XSetWindowAttributes a;
    xtk_chip *c = calloc(1, sizeof *c);

    c->label = strdup(label);
    c->w = geom.w;
    c->h = geom.h;
    a.background_pixmap = None;
    a.border_pixel = xtk_pixel(XTK_BORDER_IDLE);
    a.event_mask = KeyPressMask | KeyReleaseMask | ButtonPressMask |
                   ButtonReleaseMask | EnterWindowMask | LeaveWindowMask |
                   ExposureMask | OwnerGrabButtonMask;
    c->win = XCreateWindow(xtk_dpy(), xtk_frame(), xtk_s(geom.x), xtk_s(geom.y),
                           (unsigned)xtk_s(geom.w), (unsigned)xtk_s(geom.h),
                           (unsigned)xtk_s(m->border), CopyFromParent,
                           InputOutput, CopyFromParent,
                           CWBackPixmap | CWBorderPixel | CWEventMask, &a);
    xtk_surface_init(&c->surf, c->win, c->w, c->h);
    draw(c);
    return c;
}

void xtk_chip_map(xtk_chip *c)
{
    XRaiseWindow(xtk_dpy(), c->win);
    XMapWindow(xtk_dpy(), c->win);
}

void xtk_chip_unmap(xtk_chip *c)
{
    XUnmapWindow(xtk_dpy(), c->win);
}

static void draw(xtk_chip *c)
{
    const xtk_metrics *m = xtk_metrics_get();
    Drawable d = c->surf.pm;
    int len = (int)strlen(c->label);

    xtk_fill(d, XTK_BG, 0, 0, c->w, c->h);
    xtk_fill(d, XTK_TITLE_BG, 0, 0, c->w, m->title_h);
    xtk_draw_text(d, XTK_TITLE_FG, (c->w - len * m->char_w) / 2,
                  m->title_base, c->label, len);
}

bool xtk_chip_handle_event(xtk_chip *c, const XEvent *ev)
{
    if (ev->xany.window != c->win)
        return false;
    if (ev->type == Expose && ev->xexpose.count == 0)
        xtk_surface_present(&c->surf);
    return true;
}
