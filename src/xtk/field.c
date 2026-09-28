/*
 * An in-place edit field: a LightBlue line editor with a 1px white border
 * at a frame position, over the text it edits (Storage's Edit, recon pass
 * 14).  Keys reach it while the pointer is over it; Return reports the
 * text, Escape cancels.
 */
#include <stdlib.h>
#include <string.h>

#include "xtk/xtk.h"

static struct {
    Window win;
    xtk_surface surf;
    xtk_lineedit le;
    int w, h;
    xtk_field_fn fn;
    void *arg;
} fld;

static void draw(void)
{
    xtk_lineedit_draw(&fld.le, fld.surf.pm, fld.w, fld.h);
    xtk_surface_present(&fld.surf);
}

void xtk_field_open(int fx, int fy, int w, int h, const char *text, int cursor,
                    xtk_field_fn fn, void *arg)
{
    XSetWindowAttributes a;

    if (fld.win)
        xtk_field_close();
    fld.w = w;
    fld.h = h;
    fld.fn = fn;
    fld.arg = arg;
    a.background_pixel = xtk_pixel(XTK_FIELD_BG);
    a.border_pixel = xtk_pixel(XTK_DIALOG_FG);
    a.event_mask = KeyPressMask | ExposureMask | ButtonPressMask;
    fld.win = XCreateWindow(xtk_dpy(), xtk_frame(), xtk_s(fx), xtk_s(fy),
                            (unsigned)xtk_s(w), (unsigned)xtk_s(h),
                            (unsigned)xtk_s(1), CopyFromParent, InputOutput,
                            CopyFromParent,
                            CWBackPixel | CWBorderPixel | CWEventMask, &a);
    xtk_surface_init(&fld.surf, fld.win, w, h);
    xtk_lineedit_set(&fld.le, text);
    if (cursor >= 0 && cursor <= fld.le.len)
        fld.le.cursor = cursor;
    draw();
    XMapRaised(xtk_dpy(), fld.win);
}

void xtk_field_close(void)
{
    if (!fld.win)
        return;
    xtk_surface_free(&fld.surf);
    XDestroyWindow(xtk_dpy(), fld.win);
    fld.win = None;
}

bool xtk_field_active(void)
{
    return fld.win != None;
}

bool xtk_field_handle_event(const XEvent *ev)
{
    if (!fld.win || ev->xany.window != fld.win)
        return false;
    if (ev->type == Expose && ev->xexpose.count == 0) {
        xtk_surface_present(&fld.surf);
    } else if (ev->type == KeyPress) {
        int r = xtk_lineedit_key(&fld.le, &ev->xkey);

        if (r == 0) {
            draw();
        } else {
            char text[sizeof fld.le.text];
            xtk_field_fn fn = fld.fn;
            void *arg = fld.arg;

            memcpy(text, fld.le.text, sizeof text);
            xtk_field_close();
            if (fn)
                fn(r > 0 ? text : NULL, arg);
        }
    }
    return true;
}
