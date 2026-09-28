/*
 * Single-line text editing for dialog fields and the inline strip
 * (recon pass 11 trace): the field is filled LightBlue, the text drawn in
 * black from x=0 at baseline 11, and the cursor is an 8x12 box outlined in
 * the cursor colour at column `cursor`.
 */
#include <string.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>

#include "xtk/xtk.h"

void xtk_lineedit_set(xtk_lineedit *le, const char *text)
{
    size_t n = text ? strlen(text) : 0;

    if (n >= sizeof le->text)
        n = sizeof le->text - 1;
    memcpy(le->text, text ? text : "", n);
    le->text[n] = '\0';
    le->len = (int)n;
    le->cursor = le->len;
}

int xtk_lineedit_key(xtk_lineedit *le, const XKeyEvent *kev)
{
    char buf[8];
    KeySym ks;
    int n = XLookupString((XKeyEvent *)kev, buf, sizeof buf, &ks, NULL);

    switch (ks) {
    case XK_Return:
    case XK_KP_Enter:
        return 1;
    case XK_Escape:
        return -1;
    case XK_BackSpace:
        if (le->cursor > 0) {
            memmove(le->text + le->cursor - 1, le->text + le->cursor,
                    (size_t)(le->len - le->cursor + 1));
            le->cursor--;
            le->len--;
        }
        return 0;
    case XK_Delete:
        if (le->cursor < le->len) {
            memmove(le->text + le->cursor, le->text + le->cursor + 1,
                    (size_t)(le->len - le->cursor));
            le->len--;
        }
        return 0;
    case XK_Left:
        if (le->cursor > 0)
            le->cursor--;
        return 0;
    case XK_Right:
        if (le->cursor < le->len)
            le->cursor++;
        return 0;
    case XK_Home:
        le->cursor = 0;
        return 0;
    case XK_End:
        le->cursor = le->len;
        return 0;
    }
    if (n == 1 && buf[0] >= ' ' && buf[0] < 127 &&
        le->len < (int)sizeof le->text - 1) {
        memmove(le->text + le->cursor + 1, le->text + le->cursor,
                (size_t)(le->len - le->cursor + 1));
        le->text[le->cursor++] = buf[0];
        le->len++;
    }
    return 0;
}

void xtk_lineedit_draw(const xtk_lineedit *le, Drawable d, int w, int h)
{
    const xtk_metrics *m = xtk_metrics_get();
    Display *dpy = xtk_dpy();
    GC gc = xtk_gc();

    xtk_fill(d, XTK_FIELD_BG, 0, 0, w, h);
    xtk_draw_text(d, XTK_FIELD_FG, 0, m->ascent, le->text, le->len);
    XSetForeground(dpy, gc, xtk_pixel(XTK_CURSOR));
    XDrawRectangle(dpy, d, gc, le->cursor * m->char_w, 0,
                   (unsigned)m->char_w, (unsigned)(m->line_h - 1));
}
