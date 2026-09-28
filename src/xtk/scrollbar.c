/*
 * Scrollbars (recon passes 5 and 9).
 *
 * A vertical bar is a child of its pane at (w-11, -2), 9 x h with a 2px
 * border, so the pane clips its outer border and only the 2px left edge and
 * the 9px trough show.  Two 9x9 arrow windows sit at its top and bottom.
 * The thumb is filled #5151fb and outlined in black; it is drawn wider than
 * the bar so only its top and bottom lines are visible.
 *
 * A horizontal bar mirrors it at (-2, h-11), w x 9, arrows at (0,0) and
 * (w-9, 0).  Neither bar is shortened when both are shown (recon pass 10).
 * In the mono schemes the arrows are filled with the foreground colour
 * (recon pass 11 corrects pass 8).
 */
#include <stdlib.h>

#include "xtk/xtk.h"

struct part {
    Window win;
    xtk_surface surf;
};

struct xtk_scrollbar {
    struct part bar;
    struct part up, down;    /* left/right for a horizontal bar */
    bool vertical;
};

static void make_part(struct part *pt, Window parent, int w, int h, int border)
{
    XSetWindowAttributes a;

    a.background_pixmap = None;
    a.border_pixel = xtk_pixel(XTK_BORDER_IDLE);
    a.event_mask = ButtonPressMask | ButtonReleaseMask | EnterWindowMask |
                   LeaveWindowMask | ExposureMask | OwnerGrabButtonMask;
    pt->win = XCreateWindow(xtk_dpy(), parent, 0, 0,
                            (unsigned)xtk_s(w), (unsigned)xtk_s(h),
                            (unsigned)xtk_s(border), CopyFromParent,
                            InputOutput, CopyFromParent,
                            CWBackPixmap | CWBorderPixel | CWEventMask, &a);
    xtk_surface_init(&pt->surf, pt->win, w, h);
}

static void fill_outline(Drawable d, XPoint *pts, int n, bool fill)
{
    Display *dpy = xtk_dpy();
    GC gc = xtk_gc();

    if (fill) {
        XSetForeground(dpy, gc, xtk_pixel(XTK_THUMB));
        XFillPolygon(dpy, d, gc, pts, n, Convex, CoordModeOrigin);
    }
    XSetForeground(dpy, gc, xtk_pixel(XTK_OUTLINE));
    XDrawLines(dpy, d, gc, pts, n, CoordModeOrigin);
}

static void draw_bar(xtk_scrollbar *sb)
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
    xtk_surface *s = &sb->bar.surf;

    xtk_fill_tiled(s->pm, xtk_trough_tile(), 0, 0, s->w, s->h);
    fill_outline(s->pm, sb->vertical ? v : h, 5, true);
    xtk_surface_present(s);
}

static void draw_arrow(struct part *pt, char dir)
{
    XPoint up[4]    = { { 0, 8 }, { 4, 0 }, { 8, 8 }, { 0, 8 } };
    XPoint down[4]  = { { 8, 0 }, { 4, 8 }, { 0, 0 }, { 8, 0 } };
    XPoint left[4]  = { { 8, 0 }, { 0, 4 }, { 8, 8 }, { 8, 0 } };
    XPoint right[4] = { { 0, 0 }, { 8, 4 }, { 0, 8 }, { 0, 0 } };
    XPoint *p = dir == 'u' ? up : dir == 'd' ? down : dir == 'l' ? left : right;

    xtk_fill_tiled(pt->surf.pm, xtk_trough_tile(), 0, 0, pt->surf.w, pt->surf.h);
    fill_outline(pt->surf.pm, p, 4, true);
    xtk_surface_present(&pt->surf);
}

static void draw(xtk_scrollbar *sb)
{
    draw_bar(sb);
    draw_arrow(&sb->up, sb->vertical ? 'u' : 'l');
    draw_arrow(&sb->down, sb->vertical ? 'd' : 'r');
}

xtk_scrollbar *xtk_scrollbar_create(Window parent, bool vertical)
{
    const xtk_metrics *m = xtk_metrics_get();
    xtk_scrollbar *sb = calloc(1, sizeof *sb);

    sb->vertical = vertical;
    make_part(&sb->bar, parent, 16, 16, m->border);
    make_part(&sb->up, sb->bar.win, m->sb_w, m->sb_w, 0);
    make_part(&sb->down, sb->bar.win, m->sb_w, m->sb_w, 0);
    draw(sb);
    return sb;
}

void xtk_scrollbar_place(xtk_scrollbar *sb, int parent_w, int parent_h)
{
    Display *dpy = xtk_dpy();
    const xtk_metrics *m = xtk_metrics_get();
    int inset = m->sb_w + m->border;             /* 11 */
    int bx, by, bw, bh, dx, dy;

    if (sb->vertical) {
        bx = parent_w - inset; by = -m->border; bw = m->sb_w; bh = parent_h;
        dx = 0; dy = parent_h - m->sb_w;
    } else {
        bx = -m->border; by = parent_h - inset; bw = parent_w; bh = m->sb_w;
        dx = parent_w - m->sb_w; dy = 0;
    }
    XMoveResizeWindow(dpy, sb->bar.win, xtk_s(bx), xtk_s(by),
                      (unsigned)xtk_s(bw), (unsigned)xtk_s(bh));
    XMoveWindow(dpy, sb->up.win, 0, 0);
    XMoveWindow(dpy, sb->down.win, xtk_s(dx), xtk_s(dy));
    xtk_surface_resize(&sb->bar.surf, bw, bh);
    draw(sb);
}

void xtk_scrollbar_unmap(xtk_scrollbar *sb)
{
    XUnmapWindow(xtk_dpy(), sb->bar.win);
}

void xtk_scrollbar_map(xtk_scrollbar *sb)
{
    XMapWindow(xtk_dpy(), sb->up.win);
    XMapWindow(xtk_dpy(), sb->down.win);
    XMapWindow(xtk_dpy(), sb->bar.win);
}

bool xtk_scrollbar_handle_event(xtk_scrollbar *sb, const XEvent *ev)
{
    struct part *pt;
    Window w = ev->xany.window;

    if (w == sb->bar.win)
        pt = &sb->bar;
    else if (w == sb->up.win)
        pt = &sb->up;
    else if (w == sb->down.win)
        pt = &sb->down;
    else
        return false;
    if (ev->type == Expose && ev->xexpose.count == 0)
        xtk_surface_present(&pt->surf);
    return true;
}
