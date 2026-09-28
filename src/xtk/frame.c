/*
 * The single top-level frame that holds every pane (recon pass 1).
 */
#include <X11/Xutil.h>

#include "xtk/xtk.h"

static Window frame;

Window xtk_frame_create(xtk_rect geom, const char *title,
                        const char *icon_name, int argc, char **argv)
{
    Display *dpy = xtk_dpy();
    XSetWindowAttributes a;
    XSizeHints *size;
    XClassHint *cls;
    Atom wm_delete;

    a.background_pixmap = xtk_frame_tile();
    a.border_pixel = xtk_pixel(XTK_BORDER_IDLE);
    a.cursor = xtk_pointer();
    a.event_mask = StructureNotifyMask;
    frame = XCreateWindow(dpy, xtk_root(), geom.x, geom.y,
                          (unsigned)geom.w, (unsigned)geom.h,
                          (unsigned)xtk_metrics_get()->border,
                          CopyFromParent, InputOutput, CopyFromParent,
                          CWBackPixmap | CWBorderPixel | CWCursor | CWEventMask,
                          &a);

    XStoreName(dpy, frame, title);
    XSetIconName(dpy, frame, icon_name);
    XSetCommand(dpy, frame, argv, argc);

    size = XAllocSizeHints();
    size->flags = USPosition | USSize | PMinSize;
    size->x = geom.x;
    size->y = geom.y;
    size->width = geom.w;
    size->height = geom.h;
    size->min_width = 64;
    size->min_height = 64;
    XSetWMNormalHints(dpy, frame, size);
    XFree(size);

    cls = XAllocClassHint();
    cls->res_name = (char *)"dbxl";
    cls->res_class = (char *)"Dbxl";
    XSetClassHint(dpy, frame, cls);
    XFree(cls);

    wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, frame, &wm_delete, 1);
    return frame;
}

Window xtk_frame(void)
{
    return frame;
}
