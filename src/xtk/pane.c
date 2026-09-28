/*
 * Panes: child windows of the frame with a 2px X border, a 12px title bar
 * and a list of text lines (recon passes 1 and 9).
 *
 * Drawing follows xldb's requests exactly: the content area (from y = 13)
 * is filled with the background, each visible line is drawn as a full-width
 * space-padded string at baseline 24 + 13n, a selected line gets a 13px
 * Cyan bar with black text, and the title bar (0..11) is filled MediumBlue
 * with the title centred.  Row 12 is left as the background colour (xldb
 * leaves it to the window background).  Everything is drawn into a 1x
 * backing surface and presented at the display scale.
 */
#include <stdlib.h>
#include <string.h>

#include "xtk/xtk.h"

struct xtk_pane {
    Window win;
    xtk_surface surf;
    char *title;
    int w, h;
    char **lines;
    int nlines;
    int selected;                /* -1 = none */
    xtk_scrollbar *vsb, *hsb;
    int scrollbars;
};

static void redraw(xtk_pane *p);

xtk_pane *xtk_pane_create(const char *title, xtk_rect geom, int scrollbars)
{
    const xtk_metrics *m = xtk_metrics_get();
    XSetWindowAttributes a;
    xtk_pane *p = calloc(1, sizeof *p);

    p->title = strdup(title);
    p->w = geom.w;
    p->h = geom.h;
    p->selected = -1;
    p->scrollbars = scrollbars;

    a.background_pixmap = None;
    a.border_pixel = xtk_pixel(XTK_BORDER_IDLE);
    a.event_mask = KeyPressMask | KeyReleaseMask | ButtonPressMask |
                   ButtonReleaseMask | EnterWindowMask | LeaveWindowMask |
                   ExposureMask | OwnerGrabButtonMask;
    p->win = XCreateWindow(xtk_dpy(), xtk_frame(), xtk_s(geom.x), xtk_s(geom.y),
                           (unsigned)xtk_s(geom.w), (unsigned)xtk_s(geom.h),
                           (unsigned)xtk_s(m->border), CopyFromParent,
                           InputOutput, CopyFromParent,
                           CWBackPixmap | CWBorderPixel | CWEventMask, &a);
    xtk_surface_init(&p->surf, p->win, p->w, p->h);

    /* xldb creates both bars for every pane and maps only the enabled ones. */
    p->hsb = xtk_scrollbar_create(p->win, false);
    p->vsb = xtk_scrollbar_create(p->win, true);
    if (scrollbars & XTK_SB_VERTICAL) {
        xtk_scrollbar_place(p->vsb, p->w, p->h);
        xtk_scrollbar_map(p->vsb);
    }
    if (scrollbars & XTK_SB_HORIZONTAL) {
        xtk_scrollbar_place(p->hsb, p->w, p->h);
        xtk_scrollbar_map(p->hsb);
    }
    redraw(p);
    return p;
}

void xtk_pane_set_title(xtk_pane *p, const char *title)
{
    free(p->title);
    p->title = strdup(title);
    redraw(p);
}

void xtk_pane_set_lines(xtk_pane *p, const char *const *lines, int n)
{
    for (int i = 0; i < p->nlines; i++)
        free(p->lines[i]);
    free(p->lines);
    p->lines = calloc((size_t)(n > 0 ? n : 1), sizeof *p->lines);
    for (int i = 0; i < n; i++)
        p->lines[i] = strdup(lines[i]);
    p->nlines = n;
    redraw(p);
}

void xtk_pane_set_selected(xtk_pane *p, int line)
{
    p->selected = line;
    redraw(p);
}

void xtk_pane_map(xtk_pane *p)
{
    XRaiseWindow(xtk_dpy(), p->win);
    XMapWindow(xtk_dpy(), p->win);
}

void xtk_pane_unmap(xtk_pane *p)
{
    XUnmapWindow(xtk_dpy(), p->win);
}

Window xtk_pane_window(const xtk_pane *p)
{
    return p->win;
}

/* Copy `s` into buf padded with spaces to exactly `cols` characters. */
static void pad(char *buf, int cols, const char *s)
{
    int n = s ? (int)strlen(s) : 0;

    if (n > cols)
        n = cols;
    memcpy(buf, s ? s : "", (size_t)n);
    memset(buf + n, ' ', (size_t)(cols - n));
}

static void redraw(xtk_pane *p)
{
    const xtk_metrics *m = xtk_metrics_get();
    Drawable d = p->surf.pm;
    int top = m->title_h + 1;                    /* 13 */
    int cols = p->w / m->char_w;
    int rows = (p->h - top) / m->line_h;
    int tlen = (int)strlen(p->title);
    char *buf = malloc((size_t)cols + 1);

    xtk_fill(d, XTK_BG, 0, 0, p->w, p->h);
    for (int i = 0; i < rows; i++) {
        int y = top + i * m->line_h;
        enum xtk_color fg = XTK_FG;

        if (i == p->selected) {
            xtk_fill(d, XTK_SELECT_BG, 0, y, p->w, m->line_h);
            fg = XTK_SELECT_FG;
        }
        pad(buf, cols, i < p->nlines ? p->lines[i] : NULL);
        xtk_draw_text(d, fg, 0, y + m->ascent, buf, cols);
    }
    free(buf);

    xtk_fill(d, XTK_TITLE_BG, 0, 0, p->w, m->title_h);
    xtk_draw_text(d, XTK_TITLE_FG, (p->w - tlen * m->char_w) / 2,
                  m->title_base, p->title, tlen);
    xtk_surface_present(&p->surf);
}

static void set_active(xtk_pane *p, bool active)
{
    XSetWindowBorder(xtk_dpy(), p->win,
                     xtk_pixel(active ? XTK_BORDER_ACTIVE : XTK_BORDER_IDLE));
}

bool xtk_pane_handle_event(xtk_pane *p, const XEvent *ev)
{
    if (xtk_scrollbar_handle_event(p->vsb, ev) ||
        xtk_scrollbar_handle_event(p->hsb, ev))
        return true;
    if (ev->xany.window != p->win)
        return false;

    switch (ev->type) {
    case Expose:
        if (ev->xexpose.count == 0)
            xtk_surface_present(&p->surf);
        break;
    case EnterNotify:
        set_active(p, true);
        break;
    case LeaveNotify:
        /* Moving into our own scrollbar keeps the pane active. */
        if (ev->xcrossing.detail != NotifyInferior)
            set_active(p, false);
        break;
    }
    return true;
}
