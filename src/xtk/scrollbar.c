/*
 * Scrollbars (recon passes 5 and 9).
 *
 * A vertical bar is a child of its pane at (w-11, -2), 9 x h with a 2px
 * border, so the pane clips its outer border and only the 2px left edge and
 * the 9px trough show.  Two 9x9 arrow windows sit at its top and bottom.
 * The thumb is filled #5151fb and outlined in black; it is drawn wider than
 * the bar so only its top and bottom lines are visible.
 *
 * Horizontal bars (only the Help pane has one by default) have not been
 * observed yet; their placement and arrows mirror the vertical bar and must
 * be checked against xldb before milestone 2 is done.
 */
#include <stdlib.h>

#include "xtk/xtk.h"

struct xtk_scrollbar {
    Window bar;
    Window up, down;         /* left/right for a horizontal bar */
    bool vertical;
    int len;                 /* length of the bar along its axis */
};

static Window make_window(Window parent, int w, int h, unsigned border)
{
    XSetWindowAttributes a;

    a.background_pixmap = xtk_trough_tile();
    a.border_pixel = xtk_pixel(XTK_BORDER_IDLE);
    a.event_mask = ButtonPressMask | ButtonReleaseMask | EnterWindowMask |
                   LeaveWindowMask | ExposureMask | OwnerGrabButtonMask;
    return XCreateWindow(xtk_dpy(), parent, 0, 0, (unsigned)w, (unsigned)h,
                         border, CopyFromParent, InputOutput, CopyFromParent,
                         CWBackPixmap | CWBorderPixel | CWEventMask, &a);
}

xtk_scrollbar *xtk_scrollbar_create(Window parent, bool vertical)
{
    const xtk_metrics *m = xtk_metrics_get();
    xtk_scrollbar *sb = calloc(1, sizeof *sb);

    sb->vertical = vertical;
    sb->bar = make_window(parent, 16, 16, (unsigned)m->border);
    sb->up = make_window(sb->bar, m->sb_w, m->sb_w, 0);
    sb->down = make_window(sb->bar, m->sb_w, m->sb_w, 0);
    return sb;
}

void xtk_scrollbar_place(xtk_scrollbar *sb, int parent_w, int parent_h)
{
    Display *dpy = xtk_dpy();
    const xtk_metrics *m = xtk_metrics_get();
    int inset = m->sb_w + m->border;             /* 11 */

    if (sb->vertical) {
        sb->len = parent_h;
        XMoveResizeWindow(dpy, sb->bar, parent_w - inset, -m->border,
                          (unsigned)m->sb_w, (unsigned)parent_h);
        XMoveWindow(dpy, sb->up, 0, 0);
        XMoveWindow(dpy, sb->down, 0, parent_h - m->sb_w);
    } else {
        sb->len = parent_w;
        XMoveResizeWindow(dpy, sb->bar, -m->border, parent_h - inset,
                          (unsigned)parent_w, (unsigned)m->sb_w);
        XMoveWindow(dpy, sb->up, 0, 0);
        XMoveWindow(dpy, sb->down, parent_w - m->sb_w, 0);
    }
}

void xtk_scrollbar_map(xtk_scrollbar *sb)
{
    XMapWindow(xtk_dpy(), sb->up);
    XMapWindow(xtk_dpy(), sb->down);
    XMapWindow(xtk_dpy(), sb->bar);
}

static void fill_outline(Drawable d, XPoint *pts, int n)
{
    Display *dpy = xtk_dpy();
    GC gc = xtk_gc();

    XSetForeground(dpy, gc, xtk_pixel(XTK_THUMB));
    XFillPolygon(dpy, d, gc, pts, n, Convex, CoordModeOrigin);
    XSetForeground(dpy, gc, xtk_pixel(XTK_OUTLINE));
    XDrawLines(dpy, d, gc, pts, n, CoordModeOrigin);
}

static void draw_thumb(xtk_scrollbar *sb)
{
    /*
     * Empty or fully visible content: xldb draws the thumb from 13 to 25
     * (just below the arrow).  Proportional thumbs arrive with content
     * scrolling in a later milestone.
     */
    int a = 13, b = 25;
    XPoint v[5] = { { -1, (short)a }, { 28, (short)a }, { 28, (short)b },
                    { -1, (short)b }, { -1, (short)a } };
    XPoint h[5] = { { (short)a, -1 }, { (short)b, -1 }, { (short)b, 28 },
                    { (short)a, 28 }, { (short)a, -1 } };

    fill_outline(sb->bar, sb->vertical ? v : h, 5);
}

static void draw_arrow(Window w, char dir)
{
    XPoint up[4]    = { { 0, 8 }, { 4, 0 }, { 8, 8 }, { 0, 8 } };
    XPoint down[4]  = { { 8, 0 }, { 4, 8 }, { 0, 0 }, { 8, 0 } };
    XPoint left[4]  = { { 8, 0 }, { 0, 4 }, { 8, 8 }, { 8, 0 } };
    XPoint right[4] = { { 0, 8 }, { 8, 4 }, { 0, 0 }, { 0, 8 } };
    XPoint *p = dir == 'u' ? up : dir == 'd' ? down : dir == 'l' ? left : right;

    fill_outline(w, p, 4);
}

bool xtk_scrollbar_handle_event(xtk_scrollbar *sb, const XEvent *ev)
{
    Window w = ev->xany.window;

    if (w != sb->bar && w != sb->up && w != sb->down)
        return false;
    if (ev->type == Expose && ev->xexpose.count == 0) {
        if (w == sb->bar)
            draw_thumb(sb);
        else if (w == sb->up)
            draw_arrow(w, sb->vertical ? 'u' : 'l');
        else
            draw_arrow(w, sb->vertical ? 'd' : 'r');
    }
    return true;
}
