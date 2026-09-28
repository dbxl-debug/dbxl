/*
 * The single top-level frame that holds every pane (recon pass 1).
 * Geometry is given in logical pixels and enlarged by the scale.
 */
#include <X11/Xutil.h>

#include "xtk/xtk.h"

static Window frame;
static xtk_rect fgeom;              /* logical, excluding the border */
static Cursor busy;

Window xtk_frame_create(xtk_rect geom, const char *title,
                        const char *icon_name, int argc, char **argv)
{
    Display *dpy = xtk_dpy();
    XSetWindowAttributes a;
    XSizeHints *size;
    XClassHint *cls;
    XWMHints *wm;
    Atom wm_delete;

    fgeom = geom;
    a.background_pixmap = xtk_frame_tile();
    a.border_pixel = xtk_pixel(XTK_BORDER_IDLE);
    a.cursor = xtk_pointer();
    a.event_mask = StructureNotifyMask;
    frame = XCreateWindow(dpy, xtk_root(), xtk_s(geom.x), xtk_s(geom.y),
                          (unsigned)xtk_s(geom.w), (unsigned)xtk_s(geom.h),
                          (unsigned)xtk_s(xtk_metrics_get()->border),
                          CopyFromParent, InputOutput, CopyFromParent,
                          CWBackPixmap | CWBorderPixel | CWCursor | CWEventMask,
                          &a);

    XStoreName(dpy, frame, title);
    XSetIconName(dpy, frame, icon_name);
    XSetCommand(dpy, frame, argv, argc);

    size = XAllocSizeHints();
    size->flags = USPosition | USSize | PMinSize;
    size->x = xtk_s(geom.x);
    size->y = xtk_s(geom.y);
    size->width = xtk_s(geom.w);
    size->height = xtk_s(geom.h);
    size->min_width = xtk_s(64);
    size->min_height = xtk_s(64);
    XSetWMNormalHints(dpy, frame, size);
    XFree(size);

    cls = XAllocClassHint();
    cls->res_name = (char *)"dbxl";
    cls->res_class = (char *)"Dbxl";
    XSetClassHint(dpy, frame, cls);
    XFree(cls);

    /* xldb sets its 64x64 bug bitmap as the icon (recon pass 10). */
    wm = XAllocWMHints();
    wm->flags = IconPixmapHint;
    wm->icon_pixmap = xtk_glyph_icon();
    XSetWMHints(dpy, frame, wm);
    XFree(wm);

    wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, frame, &wm_delete, 1);
    busy = xtk_glyph_busy_cursor();
    return frame;
}

Window xtk_frame(void)
{
    return frame;
}

int xtk_frame_w(void) { return fgeom.w; }
int xtk_frame_h(void) { return fgeom.h; }

void xtk_frame_busy(bool on)
{
    XDefineCursor(xtk_dpy(), frame, on ? busy : xtk_pointer());
}

void xtk_root_to_frame(int rx, int ry, int *fx, int *fy)
{
    Window child;
    int x, y;

    XTranslateCoordinates(xtk_dpy(), xtk_root(), frame, rx, ry, &x, &y, &child);
    *fx = x >= 0 ? x / xtk_scale() : -((-x + xtk_scale() - 1) / xtk_scale());
    *fy = y >= 0 ? y / xtk_scale() : -((-y + xtk_scale() - 1) / xtk_scale());
}

void xtk_warp_frame(int fx, int fy)
{
    int n = xtk_scale();

    /* The centre of the enlarged logical pixel. */
    XWarpPointer(xtk_dpy(), None, frame, 0, 0, 0, 0,
                 fx * n + n / 2, fy * n + n / 2);
}

void xtk_pointer_frame(int *fx, int *fy)
{
    Window r, c;
    int rx, ry, wx, wy;
    unsigned int mask;

    XQueryPointer(xtk_dpy(), frame, &r, &c, &rx, &ry, &wx, &wy, &mask);
    xtk_root_to_frame(rx, ry, fx, fy);
}

xtk_rect xtk_frame_geometry(void)
{
    Window child;
    int x = 0, y = 0, n = xtk_scale();

    /* The frame's outer corner on the screen, logical. */
    XTranslateCoordinates(xtk_dpy(), xtk_frame(),
                          DefaultRootWindow(xtk_dpy()), 0, 0, &x, &y, &child);
    return (xtk_rect){ x / n - xtk_metrics_get()->border,
                       y / n - xtk_metrics_get()->border, fgeom.w, fgeom.h };
}
