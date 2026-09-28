/*
 * Backing pixmaps and integer scaling.
 *
 * Every window that dbxl draws keeps its content in a pixmap at 1x
 * (logical) size.  Presenting copies it to the window: a plain XCopyArea
 * at scale 1, or at scale N an XRender composite through a projective
 * transform that divides by N with nearest-neighbour filtering, so each
 * logical pixel becomes an exact NxN block.
 */
#include <X11/extensions/Xrender.h>

#include "xtk/xtk.h"

static void create(xtk_surface *s)
{
    Display *dpy = xtk_dpy();
    int scr = DefaultScreen(dpy);

    s->pm = XCreatePixmap(dpy, s->win, (unsigned)s->w, (unsigned)s->h,
                          (unsigned)DefaultDepth(dpy, scr));
    if (xtk_scale() > 1) {
        XRenderPictFormat *fmt =
            XRenderFindVisualFormat(dpy, DefaultVisual(dpy, scr));
        XTransform t = { { { XDoubleToFixed(1), 0, 0 },
                           { 0, XDoubleToFixed(1), 0 },
                           { 0, 0, XDoubleToFixed(xtk_scale()) } } };

        s->src_pict = XRenderCreatePicture(dpy, s->pm, fmt, 0, NULL);
        XRenderSetPictureTransform(dpy, s->src_pict, &t);
        XRenderSetPictureFilter(dpy, s->src_pict, FilterNearest, NULL, 0);
        s->dst_pict = XRenderCreatePicture(dpy, s->win, fmt, 0, NULL);
    }
}

static void destroy(xtk_surface *s)
{
    Display *dpy = xtk_dpy();

    if (s->src_pict)
        XRenderFreePicture(dpy, s->src_pict);
    if (s->dst_pict)
        XRenderFreePicture(dpy, s->dst_pict);
    if (s->pm)
        XFreePixmap(dpy, s->pm);
    s->src_pict = s->dst_pict = 0;
    s->pm = None;
}

void xtk_surface_init(xtk_surface *s, Window win, int w, int h)
{
    s->win = win;
    s->w = w > 0 ? w : 1;
    s->h = h > 0 ? h : 1;
    s->src_pict = s->dst_pict = 0;
    create(s);
}

void xtk_surface_resize(xtk_surface *s, int w, int h)
{
    destroy(s);
    s->w = w > 0 ? w : 1;
    s->h = h > 0 ? h : 1;
    create(s);
}

void xtk_surface_present(xtk_surface *s)
{
    Display *dpy = xtk_dpy();

    if (xtk_scale() == 1)
        XCopyArea(dpy, s->pm, s->win, xtk_gc(), 0, 0,
                  (unsigned)s->w, (unsigned)s->h, 0, 0);
    else
        XRenderComposite(dpy, PictOpSrc, s->src_pict, None, s->dst_pict,
                         0, 0, 0, 0, 0, 0,
                         (unsigned)xtk_s(s->w), (unsigned)xtk_s(s->h));
}

void xtk_surface_free(xtk_surface *s)
{
    destroy(s);
}
