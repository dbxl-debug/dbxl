/*
 * Dialogs (recon pass 11 trace).
 *
 * Confirm: 400 x 60, centred in the frame; message centred left of the
 * buttons at baseline 34.  Prompt: frame width x 60 with its top at the
 * frame's vertical middle; message centred left of the buttons at baseline
 * 21 and a text field (w - 96) x 13 at (9, 36).
 *
 * Compact prompt (the Edit dialog, recon pass 13): 400 x 60 at a given
 * frame position (the variable menu's), a prompt's field and baseline 21,
 * but the message centred by pixels like a confirm.
 *
 * All have a 2px border in the dialog foreground and two 64 x 13 buttons
 * with a 1px border at (w - 75, 9) and (w - 75, 36), their labels centred
 * at baseline 11.  Opening warps the pointer onto the first button at
 * (61, 10); closing warps it back to the anchor (the click that opened
 * the dialog).  Return = first button, Escape = second.
 */
#include <stdlib.h>
#include <string.h>

#include "xtk/xtk.h"

#define BTN_W 64
#define BTN_H 13

struct part {
    Window win;
    xtk_surface surf;
    int w, h;
};

static struct dialog {
    bool open;
    bool prompt;
    bool compact;
    struct part box, b1, b2, field;
    char *message, *l1, *l2;
    xtk_lineedit le;
    int ax, ay;
    xtk_dialog_fn fn;
    void *arg;
} dlg;

static void make(struct part *pt, Window parent, int x, int y, int w, int h,
                 int border, enum xtk_color bg)
{
    XSetWindowAttributes a;

    pt->w = w;
    pt->h = h;
    a.background_pixel = xtk_pixel(bg);
    a.border_pixel = xtk_pixel(XTK_DIALOG_FG);
    a.event_mask = KeyPressMask | KeyReleaseMask | ButtonPressMask |
                   ButtonReleaseMask | ExposureMask | OwnerGrabButtonMask;
    pt->win = XCreateWindow(xtk_dpy(), parent, xtk_s(x), xtk_s(y),
                            (unsigned)xtk_s(w), (unsigned)xtk_s(h),
                            (unsigned)xtk_s(border), CopyFromParent,
                            InputOutput, CopyFromParent,
                            CWBackPixel | CWBorderPixel | CWEventMask, &a);
    xtk_surface_init(&pt->surf, pt->win, w, h);
}

static void draw_button(struct part *pt, const char *label)
{
    const xtk_metrics *m = xtk_metrics_get();
    int len = (int)strlen(label);

    xtk_fill(pt->surf.pm, XTK_BG, 0, 0, pt->w, pt->h);
    xtk_draw_text(pt->surf.pm, XTK_FG, (pt->w - len * m->char_w) / 2, 11,
                  label, len);
    xtk_surface_present(&pt->surf);
}

static void draw_box(void)
{
    const xtk_metrics *m = xtk_metrics_get();
    int len = (int)strlen(dlg.message);
    int x;

    /*
     * Centred in the area left of the buttons: a prompt by whole character
     * columns (360 for "Enter function name", 328 for "Enter the target
     * file name"), a confirm by pixels rounding half up (98 for "Exit from
     * xldb?").
     */
    if (dlg.prompt && !dlg.compact) {
        int cols = (dlg.box.w - 85) / m->char_w;
        x = (cols - len) / 2 * m->char_w;
    } else {
        x = (dlg.box.w - 85 - len * m->char_w + 1) / 2;
    }

    xtk_fill(dlg.box.surf.pm, XTK_DIALOG_BG, 0, 0, dlg.box.w, dlg.box.h);
    xtk_draw_text(dlg.box.surf.pm, XTK_DIALOG_FG, x, dlg.prompt ? 21 : 34,
                  dlg.message, len);
    xtk_surface_present(&dlg.box.surf);
}

static void draw_field(void)
{
    xtk_lineedit_draw(&dlg.le, dlg.field.surf.pm, dlg.field.w, dlg.field.h);
    xtk_surface_present(&dlg.field.surf);
}

static void open_dialog(bool prompt, const xtk_point *at, const char *message,
                        const char *initial, const char *b1, const char *b2,
                        int ax, int ay, xtk_dialog_fn fn, void *arg)
{
    Display *dpy = xtk_dpy();
    int fw = xtk_frame_w(), fh = xtk_frame_h();
    int w = prompt && !at ? fw : 400, h = 60;
    int x = at ? at->x : prompt ? 0 : (fw - w) / 2;
    int y = at ? at->y : prompt ? fh / 2 : (fh - h) / 2;

    if (dlg.open)
        return;
    memset(&dlg, 0, sizeof dlg);
    dlg.open = true;
    dlg.prompt = prompt;
    dlg.compact = at != NULL;
    dlg.message = strdup(message);
    dlg.l1 = strdup(b1);
    dlg.l2 = strdup(b2);
    dlg.ax = ax;
    dlg.ay = ay;
    dlg.fn = fn;
    dlg.arg = arg;
    xtk_lineedit_set(&dlg.le, initial);

    make(&dlg.box, xtk_frame(), x, y, w, h, 2, XTK_DIALOG_BG);
    make(&dlg.b1, dlg.box.win, w - 75, 9, BTN_W, BTN_H, 1, XTK_BG);
    make(&dlg.b2, dlg.box.win, w - 75, 36, BTN_W, BTN_H, 1, XTK_BG);
    if (prompt) {
        make(&dlg.field, dlg.box.win, 9, 36, w - 96, BTN_H, 1, XTK_FIELD_BG);
        XMapWindow(dpy, dlg.field.win);
        draw_field();
    }
    draw_box();
    draw_button(&dlg.b1, b1);
    draw_button(&dlg.b2, b2);
    XMapWindow(dpy, dlg.b1.win);
    XMapWindow(dpy, dlg.b2.win);
    XMapRaised(dpy, dlg.box.win);
    XWarpPointer(dpy, None, dlg.b1.win, 0, 0, 0, 0,
                 xtk_s(61) + xtk_scale() / 2, xtk_s(10) + xtk_scale() / 2);
}

void xtk_dialog_confirm(const char *message, const char *b1, const char *b2,
                        int ax, int ay, xtk_dialog_fn fn, void *arg)
{
    open_dialog(false, NULL, message, NULL, b1, b2, ax, ay, fn, arg);
}

void xtk_dialog_prompt(const char *message, const char *initial,
                       const char *b1, const char *b2, int ax, int ay,
                       xtk_dialog_fn fn, void *arg)
{
    open_dialog(true, NULL, message, initial, b1, b2, ax, ay, fn, arg);
}

void xtk_dialog_prompt_at(const char *message, const char *initial,
                          const char *b1, const char *b2, int x, int y,
                          int ax, int ay, xtk_dialog_fn fn, void *arg)
{
    xtk_point at = { x, y };

    open_dialog(true, &at, message, initial, b1, b2, ax, ay, fn, arg);
}

bool xtk_dialog_active(void)
{
    return dlg.open;
}

static void free_part(struct part *pt)
{
    if (pt->win)
        xtk_surface_free(&pt->surf);
}

static void finish(int button)
{
    xtk_dialog_fn fn = dlg.fn;
    void *arg = dlg.arg;
    char text[sizeof dlg.le.text];

    memcpy(text, dlg.le.text, sizeof text);
    /* Warp first, as xldb does: the pointer never lands on whatever the
     * dialog covered, so nothing there autoraises (recon pass 14). */
    xtk_warp_frame(dlg.ax, dlg.ay);
    free_part(&dlg.b1);
    free_part(&dlg.b2);
    free_part(&dlg.field);
    free_part(&dlg.box);
    XDestroyWindow(xtk_dpy(), dlg.box.win);
    free(dlg.message);
    free(dlg.l1);
    free(dlg.l2);
    dlg.open = false;
    if (fn)
        fn(button, text, arg);
}

bool xtk_dialog_handle_event(const XEvent *ev)
{
    Window w = ev->xany.window;

    if (!dlg.open)
        return false;
    if (w != dlg.box.win && w != dlg.b1.win && w != dlg.b2.win &&
        w != dlg.field.win) {
        /* Keys typed with the pointer elsewhere still edit the field. */
        if (ev->type != KeyPress)
            return false;
    }
    switch (ev->type) {
    case Expose:
        if (ev->xexpose.count == 0) {
            if (w == dlg.box.win)
                xtk_surface_present(&dlg.box.surf);
            else if (w == dlg.b1.win)
                xtk_surface_present(&dlg.b1.surf);
            else if (w == dlg.b2.win)
                xtk_surface_present(&dlg.b2.surf);
            else if (w == dlg.field.win)
                xtk_surface_present(&dlg.field.surf);
        }
        break;
    case KeyPress: {
        int r = xtk_lineedit_key(&dlg.le, &ev->xkey);
        if (r > 0)
            finish(0);
        else if (r < 0)
            finish(1);
        else if (dlg.prompt)
            draw_field();
        break;
    }
    case ButtonPress:
        if (w == dlg.b1.win)
            finish(0);
        else if (w == dlg.b2.win)
            finish(1);
        break;
    }
    return true;
}
