/*
 * Panes: child windows of the frame with a 2px X border, a 12px title bar
 * and a list of text lines (recon passes 1, 9, 11).
 *
 * Drawing follows xldb's requests: the content area (from y = 13) is
 * filled with the background, each visible line is drawn as a full-width
 * space-padded string at baseline 24 + 13n, a selected line gets a 13px
 * bar in the selection colours, and the title bar (0..11) is filled with
 * the title colour and the title centred.  With tagWindows the tag is also
 * drawn at x = 0 and right-aligned (11px in from the edge when there is a
 * vertical scrollbar, 1px otherwise).  Row 12 is the background colour.
 * Everything is drawn into a 1x backing surface and presented at the
 * display scale.
 *
 * Input: left click on the title bar or content, and keys, go to the
 * owner's callback; a right press starts a move/resize (gesture.c); a
 * middle press raises the pane.  Entering a pane makes its border the
 * active colour and, with autoraise, raises it.  Arrow keys, Home and End
 * warp the pointer by character cells, as xldb's "cursor" is the pointer.
 */
#include <stdlib.h>
#include <string.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>

#include "xtk/xtk.h"

void xtk_gesture_press(xtk_pane *p, int x, int y, int fx, int fy);

struct strip {
    Window win;
    xtk_surface surf;
    xtk_lineedit le;
    int w;
};

struct xtk_pane {
    Window win;
    xtk_surface surf;
    char *title;
    char *tag;
    xtk_rect geom;
    char **lines;
    int nlines;
    int selected;                /* line index, -1 = none */
    int top;                     /* first visible line */
    unsigned char *marks;        /* margin glyphs per line */
    int nmarks;
    unsigned char *roles;        /* per line: XTK_ROLE_* */
    int nroles;
    xtk_scrollbar *vsb, *hsb;
    int scrollbars;
    bool mapped;
    struct strip strip;
    xtk_pane_fn fn;
    void *arg;
};

static bool autoraise = true;

static void redraw(xtk_pane *p);
static void scrolled(xtk_scrollbar *sb, int dir, void *arg);

xtk_pane *xtk_pane_create(const char *title, xtk_rect geom, int scrollbars)
{
    const xtk_metrics *m = xtk_metrics_get();
    XSetWindowAttributes a;
    xtk_pane *p = calloc(1, sizeof *p);

    p->title = strdup(title);
    p->geom = geom;
    p->selected = -1;

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
    xtk_surface_init(&p->surf, p->win, geom.w, geom.h);

    /*
     * xldb creates both bars for every pane, vertical first (so the
     * horizontal bar is on top where they overlap), and maps only the
     * enabled ones.
     */
    p->vsb = xtk_scrollbar_create(p->win, true);
    p->hsb = xtk_scrollbar_create(p->win, false);
    xtk_scrollbar_set_handler(p->vsb, scrolled, p);
    xtk_pane_set_scrollbars(p, scrollbars);
    return p;
}

void xtk_pane_set_handler(xtk_pane *p, xtk_pane_fn fn, void *arg)
{
    p->fn = fn;
    p->arg = arg;
}

void xtk_pane_set_title(xtk_pane *p, const char *title)
{
    free(p->title);
    p->title = strdup(title);
    redraw(p);
}

void xtk_pane_set_tag(xtk_pane *p, const char *tag)
{
    free(p->tag);
    p->tag = tag ? strdup(tag) : NULL;
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

int xtk_pane_nlines(const xtk_pane *p) { return p->nlines; }

const char *xtk_pane_line(const xtk_pane *p, int i)
{
    return i >= 0 && i < p->nlines ? p->lines[i] : "";
}

int xtk_pane_rows(const xtk_pane *p)
{
    const xtk_metrics *m = xtk_metrics_get();
    int rows = (p->geom.h - (m->title_h + 1)) / m->line_h;

    return rows > 0 ? rows : 0;
}

void xtk_pane_set_selected(xtk_pane *p, int line)
{
    p->selected = line;
    redraw(p);
}

int xtk_pane_top(const xtk_pane *p) { return p->top; }

void xtk_pane_set_top(xtk_pane *p, int top)
{
    p->top = top > 0 ? top : 0;
    redraw(p);
}

void xtk_pane_set_marks(xtk_pane *p, const unsigned char *marks, int n)
{
    free(p->marks);
    p->marks = NULL;
    p->nmarks = 0;
    if (n > 0) {
        p->marks = malloc((size_t)n);
        memcpy(p->marks, marks, (size_t)n);
        p->nmarks = n;
    }
    redraw(p);
}

void xtk_pane_set_line_roles(xtk_pane *p, const unsigned char *roles, int n)
{
    free(p->roles);
    p->roles = NULL;
    p->nroles = 0;
    if (n > 0) {
        p->roles = malloc((size_t)n);
        memcpy(p->roles, roles, (size_t)n);
        p->nroles = n;
    }
    redraw(p);
}

/*
 * The arrows scroll by a line.  Down stops once the last page is reached;
 * a view that a stop centred further down stays put (recon pass 12).
 */
static void scrolled(xtk_scrollbar *sb, int dir, void *arg)
{
    xtk_pane *p = arg;

    (void)sb;
    if (dir < 0 && p->top > 0)
        xtk_pane_set_top(p, p->top - 1);
    else if (dir > 0 && p->top < p->nlines - xtk_pane_rows(p))
        xtk_pane_set_top(p, p->top + 1);
}

/*
 * xldb's thumb: a fixed 12px thumb whose top moves from 13 over the bar's
 * length less 38, in steps of (length - 39) / range, where range is the
 * number of lines beyond one page (at least 1).  Measured with 21 and 49
 * line files in a 420px pane (recon pass 12).
 */
static int thumb_pos(const xtk_pane *p)
{
    int track = p->geom.h - 38;
    int range = p->nlines - xtk_pane_rows(p);
    long pos;

    if (range < 1)
        range = 1;
    pos = (long)(track - 1) * p->top / range;
    return 13 + (int)(pos < track ? pos : track);
}

void xtk_pane_map(xtk_pane *p)
{
    XRaiseWindow(xtk_dpy(), p->win);
    XMapWindow(xtk_dpy(), p->win);
    p->mapped = true;
}

void xtk_pane_unmap(xtk_pane *p)
{
    XUnmapWindow(xtk_dpy(), p->win);
    p->mapped = false;
}

bool xtk_pane_mapped(const xtk_pane *p) { return p->mapped; }
void xtk_pane_raise(xtk_pane *p) { XRaiseWindow(xtk_dpy(), p->win); }
void xtk_pane_lower(xtk_pane *p) { XLowerWindow(xtk_dpy(), p->win); }
xtk_rect xtk_pane_geometry(const xtk_pane *p) { return p->geom; }
Window xtk_pane_window(const xtk_pane *p) { return p->win; }
int xtk_pane_scrollbars(const xtk_pane *p) { return p->scrollbars; }
void xtk_pane_set_autoraise(bool on) { autoraise = on; }
bool xtk_pane_autoraise(void) { return autoraise; }

void xtk_pane_set_scrollbars(xtk_pane *p, int flags)
{
    p->scrollbars = flags;
    if (flags & XTK_SB_VERTICAL) {
        xtk_scrollbar_place(p->vsb, p->geom.w, p->geom.h);
        xtk_scrollbar_map(p->vsb);
    } else {
        xtk_scrollbar_unmap(p->vsb);
    }
    if (flags & XTK_SB_HORIZONTAL) {
        xtk_scrollbar_place(p->hsb, p->geom.w, p->geom.h);
        xtk_scrollbar_map(p->hsb);
    } else {
        xtk_scrollbar_unmap(p->hsb);
    }
    redraw(p);
}

void xtk_pane_set_geometry(xtk_pane *p, xtk_rect g)
{
    bool resized = g.w != p->geom.w || g.h != p->geom.h;

    p->geom = g;
    XMoveResizeWindow(xtk_dpy(), p->win, xtk_s(g.x), xtk_s(g.y),
                      (unsigned)xtk_s(g.w), (unsigned)xtk_s(g.h));
    if (resized) {
        xtk_surface_resize(&p->surf, g.w, g.h);
        xtk_pane_set_scrollbars(p, p->scrollbars);
    }
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
    int w = p->geom.w, h = p->geom.h;
    int top = m->title_h + 1;                    /* 13 */
    int cols = w / m->char_w;
    int rows = xtk_pane_rows(p);
    int tlen = (int)strlen(p->title);
    char *buf = malloc((size_t)cols + 1);

    xtk_fill(d, XTK_BG, 0, 0, w, h);
    for (int i = 0; i < rows; i++) {
        int y = top + i * m->line_h;
        int line = p->top + i;
        enum xtk_color fg = XTK_FG;

        if (line == p->selected) {
            xtk_fill(d, XTK_SELECT_BG, 0, y, w, m->line_h);
            fg = XTK_SELECT_FG;
        } else if (line < p->nroles && p->roles[line] == XTK_ROLE_INFO) {
            fg = XTK_MENUINFO_FG;
        }
        pad(buf, cols, line < p->nlines ? p->lines[line] : NULL);
        xtk_draw_text(d, fg, 0, y + m->ascent, buf, cols);
    }
    free(buf);
    /* Margin glyphs go over the text: stop signs, then the arrow. */
    for (int pass = 0; pass < 2; pass++)
        for (int i = 0; i < rows; i++) {
            int line = p->top + i;
            int mk = line < p->nmarks ? p->marks[line] : 0;
            int y = top + i * m->line_h;

            if (pass == 0 && (mk & XTK_MARK_STOP))
                xtk_glyph_draw(d, XTK_GLYPH_STOP, y);
            else if (pass == 0 && (mk & XTK_MARK_STOP_COND))
                xtk_glyph_draw(d, XTK_GLYPH_STOP_COND, y);
            else if (pass == 0 && (mk & XTK_MARK_STOP_DISABLED))
                xtk_glyph_draw(d, XTK_GLYPH_STOP_DISABLED, y);
            else if (pass == 1 && (mk & XTK_MARK_ARROW))
                xtk_glyph_draw(d, XTK_GLYPH_ARROW, y);
        }

    xtk_fill(d, XTK_TITLE_BG, 0, 0, w, m->title_h);
    xtk_draw_text(d, XTK_TITLE_FG, (w - tlen * m->char_w) / 2,
                  m->title_base, p->title, tlen);
    if (p->tag) {
        int len = (int)strlen(p->tag);
        int inset = (p->scrollbars & XTK_SB_VERTICAL) ? 11 : 1;
        int tx = (w - tlen * m->char_w) / 2;
        int rx = w - len * m->char_w - inset;

        /* Tags are left out when they would overlap the title (recon 11). */
        if (len * m->char_w <= tx && rx >= tx + tlen * m->char_w) {
            xtk_draw_text(d, XTK_TITLE_FG, 0, m->title_base, p->tag, len);
            xtk_draw_text(d, XTK_TITLE_FG, rx, m->title_base, p->tag, len);
        }
    }
    xtk_surface_present(&p->surf);
    xtk_scrollbar_set_thumb(p->vsb, thumb_pos(p));
}

static void set_active(xtk_pane *p, bool active)
{
    XSetWindowBorder(xtk_dpy(), p->win,
                     xtk_pixel(active ? XTK_BORDER_ACTIVE : XTK_BORDER_IDLE));
}

/* ---- inline strip --------------------------------------------------- */

static void strip_draw(xtk_pane *p)
{
    struct strip *s = &p->strip;

    xtk_lineedit_draw(&s->le, s->surf.pm, s->w, 13);
    xtk_surface_present(&s->surf);
}

void xtk_pane_strip_open(xtk_pane *p, int y, const char *initial)
{
    XSetWindowAttributes a;
    struct strip *s = &p->strip;

    if (s->win)
        return;
    /* xldb: a w x 13 child with a 1px border, 18px below the pointer. */
    s->w = p->geom.w;
    a.background_pixel = xtk_pixel(XTK_FIELD_BG);
    a.border_pixel = xtk_pixel(XTK_FG);
    a.event_mask = KeyPressMask | ExposureMask;
    s->win = XCreateWindow(xtk_dpy(), p->win, xtk_s(-1), xtk_s(y + 18),
                           (unsigned)xtk_s(s->w), (unsigned)xtk_s(13),
                           (unsigned)xtk_s(1), CopyFromParent, InputOutput,
                           CopyFromParent,
                           CWBackPixel | CWBorderPixel | CWEventMask, &a);
    xtk_surface_init(&s->surf, s->win, s->w, 13);
    xtk_lineedit_set(&s->le, initial);
    strip_draw(p);
    XMapWindow(xtk_dpy(), s->win);
}

static void strip_close(xtk_pane *p)
{
    struct strip *s = &p->strip;

    xtk_surface_free(&s->surf);
    XDestroyWindow(xtk_dpy(), s->win);
    s->win = None;
}

static void strip_key(xtk_pane *p, const XKeyEvent *kev)
{
    struct strip *s = &p->strip;
    int r = xtk_lineedit_key(&s->le, kev);

    if (r == 0) {
        strip_draw(p);
        return;
    }
    if (r > 0 && p->fn) {
        char text[sizeof s->le.text];
        xtk_pane_event e = { .type = XTK_PANE_STRIP_DONE };

        memcpy(text, s->le.text, sizeof text);
        strip_close(p);
        e.text = text;
        p->fn(p, &e, p->arg);
        return;
    }
    strip_close(p);
}

/* ---- keys: pointer warping and the strip ---------------------------- */

static void warp_cell(xtk_pane *p, int row, int col)
{
    const xtk_metrics *m = xtk_metrics_get();
    int cols = p->geom.w / m->char_w, rows = xtk_pane_rows(p);

    if (row >= rows) row = rows - 1;
    if (row < 0) row = 0;
    if (col >= cols) col = cols - 1;
    if (col < 0) col = 0;
    /* The centre of the character cell, inside the pane's border. */
    xtk_warp_frame(p->geom.x + 2 + col * m->char_w + m->char_w / 2,
                   p->geom.y + 2 + m->title_h + 1 + row * m->line_h + 6);
}

static void key(xtk_pane *p, const XKeyEvent *kev, int x, int y, int fx, int fy)
{
    const xtk_metrics *m = xtk_metrics_get();
    char buf[8];
    KeySym ks;
    int n = XLookupString((XKeyEvent *)kev, buf, sizeof buf - 1, &ks, NULL);
    int row = (y - (m->title_h + 1)) / m->line_h;
    int col = x / m->char_w;

    switch (ks) {
    case XK_Up:    warp_cell(p, row - 1, col); return;
    case XK_Down:  warp_cell(p, row + 1, col); return;
    case XK_Left:  warp_cell(p, row, col - 1); return;
    case XK_Right: warp_cell(p, row, col + 1); return;
    case XK_Home:  warp_cell(p, row, 0); return;
    case XK_End: {
        int len = (int)strlen(xtk_pane_line(p, p->top + row));
        warp_cell(p, row, len > 0 ? len - 1 : 0);
        return;
    }
    }
    buf[n > 0 ? n : 0] = '\0';
    if (n == 1 && strchr(":/\\?", buf[0]) && !(kev->state & Mod1Mask)) {
        xtk_pane_strip_open(p, y, buf);
        return;
    }
    if (p->fn) {
        xtk_pane_event e = { .type = XTK_PANE_KEY, .keysym = ks,
                             .state = kev->state, .x = x, .y = y,
                             .fx = fx, .fy = fy, .text = buf };
        p->fn(p, &e, p->arg);
    }
}

bool xtk_pane_handle_event(xtk_pane *p, const XEvent *ev)
{
    const xtk_metrics *m = xtk_metrics_get();
    int n = xtk_scale();
    int x, y, fx, fy;

    if (p->strip.win && ev->xany.window == p->strip.win) {
        if (ev->type == Expose && ev->xexpose.count == 0)
            xtk_surface_present(&p->strip.surf);
        else if (ev->type == KeyPress)
            strip_key(p, &ev->xkey);
        return true;
    }
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
        /* Not over an open dialog or edit field (they stay in front). */
        if (autoraise && ev->xcrossing.detail != NotifyInferior &&
            !xtk_dialog_active() && !xtk_field_active())
            xtk_pane_raise(p);
        break;
    case LeaveNotify:
        /* Moving into our own scrollbar keeps the pane active. */
        if (ev->xcrossing.detail != NotifyInferior)
            set_active(p, false);
        break;
    case KeyPress:
        if (p->strip.win) {
            strip_key(p, &ev->xkey);
            break;
        }
        x = ev->xkey.x / n;
        y = ev->xkey.y / n;
        xtk_root_to_frame(ev->xkey.x_root, ev->xkey.y_root, &fx, &fy);
        key(p, &ev->xkey, x, y, fx, fy);
        break;
    case ButtonPress:
        x = ev->xbutton.x / n;
        y = ev->xbutton.y / n;
        xtk_root_to_frame(ev->xbutton.x_root, ev->xbutton.y_root, &fx, &fy);
        if (ev->xbutton.button == Button3) {
            xtk_gesture_press(p, x, y, fx, fy);
        } else if (ev->xbutton.button == Button2) {
            xtk_pane_raise(p);
        } else if (p->fn) {
            xtk_pane_event e = { .button = (int)ev->xbutton.button,
                                 .state = ev->xbutton.state,
                                 .x = x, .y = y, .fx = fx, .fy = fy };

            if (y < m->title_h) {
                e.type = XTK_PANE_TITLE_CLICK;
            } else {
                e.type = XTK_PANE_CLICK;
                e.row = (y - (m->title_h + 1)) / m->line_h;
                e.col = x / m->char_w;
                e.line = p->top + e.row;
            }
            p->fn(p, &e, p->arg);
        }
        break;
    }
    return true;
}
