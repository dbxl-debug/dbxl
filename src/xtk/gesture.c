/*
 * Interactive move and resize (recon passes 8-10).
 *
 * Right press in a pane: its inside is split into a 3x3 grid by thirds.
 * The centre cell moves the pane; side cells resize that edge; corners
 * resize both edges.  (Horizontal centre: w/3 <= x <= 2w/3; vertical
 * centre: h/3 < y <= 2h/3.)  While moving, the outline follows the pointer
 * (xldb lags one motion event; dbxl does not).  While resizing, an edge
 * stays put until the pointer reaches it and then follows the pointer.
 *
 * Window Control "Move" / "Size" arm the same operations: the next press
 * starts them, and the edges resized are those on the pointer's side of
 * the pane; they then move by the drag distance.
 *
 * The outline is xldb's: the pane's outer box drawn on the frame with XOR
 * 0xa5a5a5 through inferiors, as four lines whose shared corners are drawn
 * twice (and so cancel).  Cursors: fleur while moving, the matching side or
 * corner shape once an edge follows the pointer.
 */
#include <X11/cursorfont.h>

#include "xtk/xtk.h"

enum { E_L = 1, E_R = 2, E_T = 4, E_B = 8 };

static struct {
    enum { G_NONE, G_ARMED_MOVE, G_ARMED_SIZE, G_MOVE, G_RESIZE_SNAP,
           G_RESIZE_DELTA } mode;
    xtk_pane *pane;
    xtk_rect orig;              /* outer box at the start: x, y, w, h */
    xtk_rect cur;               /* outer box drawn now */
    int px, py;                 /* press position, frame coords */
    int edges;
    int engaged;                /* snap resize: edges following the pointer */
    bool drawn;
    GC xor_gc;
} g;

static Cursor font_cursor(unsigned int shape)
{
    static Cursor cache[XC_num_glyphs];

    if (!cache[shape])
        cache[shape] = XCreateFontCursor(xtk_dpy(), shape);
    return cache[shape];
}

static Cursor edge_cursor(int edges)
{
    switch (edges) {
    case E_L:       return font_cursor(XC_left_side);
    case E_R:       return font_cursor(XC_right_side);
    case E_T:       return font_cursor(XC_top_side);
    case E_B:       return font_cursor(XC_bottom_side);
    case E_L | E_T: return font_cursor(XC_top_left_corner);
    case E_R | E_T: return font_cursor(XC_top_right_corner);
    case E_L | E_B: return font_cursor(XC_bottom_left_corner);
    case E_R | E_B: return font_cursor(XC_bottom_right_corner);
    }
    return font_cursor(XC_fleur);
}

void xtk_rubber_draw(xtk_rect r)
{
    Display *dpy = xtk_dpy();
    int n = xtk_scale();
    XRectangle rects[4];

    if (!g.xor_gc) {
        XGCValues v;
        v.function = GXxor;
        v.foreground = 0xa5a5a5;
        v.subwindow_mode = IncludeInferiors;
        v.graphics_exposures = False;
        g.xor_gc = XCreateGC(dpy, xtk_frame(),
                             GCFunction | GCForeground | GCSubwindowMode |
                             GCGraphicsExposures, &v);
    }
    /* Each line includes both corner pixels, as xldb's PolySegment does. */
    rects[0] = (XRectangle){ (short)(r.x * n), (short)(r.y * n),
                             (unsigned short)(r.w * n), (unsigned short)n };
    rects[1] = (XRectangle){ (short)(r.x * n), (short)((r.y + r.h - 1) * n),
                             (unsigned short)(r.w * n), (unsigned short)n };
    rects[2] = (XRectangle){ (short)(r.x * n), (short)(r.y * n),
                             (unsigned short)n, (unsigned short)(r.h * n) };
    rects[3] = (XRectangle){ (short)((r.x + r.w - 1) * n), (short)(r.y * n),
                             (unsigned short)n, (unsigned short)(r.h * n) };
    XFillRectangles(dpy, xtk_frame(), g.xor_gc, rects, 4);
}

static void outline(xtk_rect r)
{
    if (g.drawn)
        xtk_rubber_draw(g.cur);
    g.cur = r;
    xtk_rubber_draw(g.cur);
    g.drawn = true;
}

static xtk_rect outer_of(xtk_pane *p)
{
    xtk_rect in = xtk_pane_geometry(p);
    return (xtk_rect){ in.x, in.y, in.w + 4, in.h + 4 };
}

static void grab(Cursor c)
{
    XGrabPointer(xtk_dpy(), xtk_frame(), False,
                 ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                 GrabModeAsync, GrabModeAsync, xtk_frame(), c, CurrentTime);
}

static void start(xtk_pane *p, int mode, int px, int py, int edges)
{
    g.pane = p;
    g.mode = mode;
    g.orig = g.cur = outer_of(p);
    g.px = px;
    g.py = py;
    g.edges = edges;
    g.engaged = 0;
    g.drawn = false;
    grab(font_cursor(XC_fleur));
    outline(g.orig);
}

/* Called by the pane for a right-button press at inside position (x, y). */
void xtk_gesture_press(xtk_pane *p, int x, int y, int fx, int fy)
{
    xtk_rect in = xtk_pane_geometry(p);
    int edges = 0;

    if (x * 3 < in.w)
        edges |= E_L;
    else if (x * 3 > 2 * in.w)
        edges |= E_R;
    if (y * 3 <= in.h)
        edges |= E_T;
    else if (y * 3 > 2 * in.h)
        edges |= E_B;
    start(p, edges ? G_RESIZE_SNAP : G_MOVE, fx, fy, edges);
}

void xtk_pane_begin_move(xtk_pane *p)
{
    g.pane = p;
    g.mode = G_ARMED_MOVE;
    grab(font_cursor(XC_fleur));
}

void xtk_pane_begin_size(xtk_pane *p)
{
    g.pane = p;
    g.mode = G_ARMED_SIZE;
    grab(xtk_pointer());
}

bool xtk_gesture_active(void)
{
    return g.mode != G_NONE;
}

static void motion(int fx, int fy)
{
    xtk_rect r = g.orig;
    int dx = fx - g.px, dy = fy - g.py;
    int right = g.orig.x + g.orig.w - 1, bottom = g.orig.y + g.orig.h - 1;
    int left = g.orig.x, top = g.orig.y;

    switch (g.mode) {
    case G_MOVE:
        r.x += dx;
        r.y += dy;
        break;
    case G_RESIZE_DELTA:
        if (g.edges & E_L) left += dx;
        if (g.edges & E_R) right += dx;
        if (g.edges & E_T) top += dy;
        if (g.edges & E_B) bottom += dy;
        r = (xtk_rect){ left, top, right - left + 1, bottom - top + 1 };
        break;
    case G_RESIZE_SNAP: {
        int was = g.engaged;
        if ((g.edges & E_L) && fx <= left) g.engaged |= E_L;
        if ((g.edges & E_R) && fx >= right) g.engaged |= E_R;
        if ((g.edges & E_T) && fy <= top) g.engaged |= E_T;
        if ((g.edges & E_B) && fy >= bottom) g.engaged |= E_B;
        if (g.engaged != was)
            grab(edge_cursor(g.engaged));
        if (g.engaged & E_L) left = fx;
        if (g.engaged & E_R) right = fx;
        if (g.engaged & E_T) top = fy;
        if (g.engaged & E_B) bottom = fy;
        r = (xtk_rect){ left, top, right - left + 1, bottom - top + 1 };
        break;
    }
    default:
        return;
    }
    if (r.w < 16)
        r.w = 16;
    if (r.h < 16)
        r.h = 16;
    outline(r);
}

static void finish(void)
{
    xtk_rect r = g.cur;

    if (g.drawn)
        xtk_rubber_draw(g.cur);
    XUngrabPointer(xtk_dpy(), CurrentTime);
    g.mode = G_NONE;
    g.drawn = false;
    xtk_pane_set_geometry(g.pane, (xtk_rect){ r.x, r.y, r.w - 4, r.h - 4 });
    xtk_pane_raise(g.pane);
}

bool xtk_gesture_handle_event(const XEvent *ev)
{
    int fx, fy;

    if (g.mode == G_NONE)
        return false;
    switch (ev->type) {
    case ButtonPress:
        xtk_root_to_frame(ev->xbutton.x_root, ev->xbutton.y_root, &fx, &fy);
        if (g.mode == G_ARMED_MOVE) {
            start(g.pane, G_MOVE, fx, fy, 0);
        } else if (g.mode == G_ARMED_SIZE) {
            xtk_rect o = outer_of(g.pane);
            int edges = 0;
            if (fx < o.x) edges |= E_L;
            else if (fx >= o.x + o.w) edges |= E_R;
            if (fy < o.y) edges |= E_T;
            else if (fy >= o.y + o.h) edges |= E_B;
            if (!edges)
                edges = E_R | E_B;
            start(g.pane, G_RESIZE_DELTA, fx, fy, edges);
            grab(edge_cursor(edges));
        }
        return true;
    case MotionNotify:
        xtk_root_to_frame(ev->xmotion.x_root, ev->xmotion.y_root, &fx, &fy);
        motion(fx, fy);
        return true;
    case ButtonRelease:
        if (g.mode == G_MOVE || g.mode == G_RESIZE_SNAP ||
            g.mode == G_RESIZE_DELTA)
            finish();
        return true;
    }
    return false;
}
