/*
 * Cell menus (recon passes 9-11 traces).
 *
 * The menu is a frame child with a 2px border and the title colour as
 * background, w x (17 + 19 n).  The title is drawn centred in a (w-8)/8
 * column field at (4, 11).  Each item is a w x 17 child window with a 2px
 * border at (-2, 17 + 19 k), so neighbouring borders form the separator
 * lines; its text is centred at baseline 11.  The item under the pointer
 * is drawn in the highlight colours.
 *
 * Opening warps the pointer to (w/2 - 4, 4) inside the default item.
 * Leaving the menu closes it without a selection.  A selection is
 * reported to the callback, which closes the menu (warping the pointer
 * back to the anchor) or updates items and leaves it open.
 */
#include <stdlib.h>
#include <string.h>

#include "xtk/xtk.h"

#define TITLE_H 17
#define CELL_H 17
#define CELL_STEP 19

struct item {
    Window win;
    xtk_surface surf;
    char *text;
};

struct xtk_menu {
    Window win;
    xtk_surface surf;
    char *title;
    int x, y, w, h;
    int ax, ay;                  /* anchor: where the pointer returns */
    int nitems;
    struct item *items;
    int hilite;
    xtk_menu_fn fn;
    void *arg;
};

static xtk_menu *active;

static int text_w(const char *s)
{
    return (int)strlen(s) * xtk_metrics_get()->char_w;
}

static void draw_title(xtk_menu *m)
{
    const xtk_metrics *mt = xtk_metrics_get();
    int cols = (m->w - 8) / mt->char_w;
    int len = (int)strlen(m->title);
    int left = (cols - len + 1) / 2;

    if (left < 0)
        left = 0;
    xtk_fill(m->surf.pm, XTK_TITLE_BG, 0, 0, m->w, m->h);
    xtk_draw_text(m->surf.pm, XTK_TITLE_FG, 4 + left * mt->char_w, 11,
                  m->title, len);
    xtk_surface_present(&m->surf);
}

static void draw_item(xtk_menu *m, int k)
{
    struct item *it = &m->items[k];
    bool hl = k == m->hilite;
    int len = (int)strlen(it->text);

    xtk_fill(it->surf.pm, hl ? XTK_MENU_HL_BG : XTK_BG, 0, 0, m->w, CELL_H);
    xtk_draw_text(it->surf.pm, hl ? XTK_MENU_HL_FG : XTK_FG,
                  (m->w - text_w(it->text)) / 2, 11, it->text, len);
    xtk_surface_present(&it->surf);
}

static void set_hilite(xtk_menu *m, int k)
{
    int old = m->hilite;

    if (old == k)
        return;
    m->hilite = k;
    if (old >= 0)
        draw_item(m, old);
    if (k >= 0)
        draw_item(m, k);
}

static xtk_menu *create(const xtk_menu_spec *spec, xtk_menu_fn fn, void *arg)
{
    xtk_menu *m = calloc(1, sizeof *m);
    int w = spec->width;

    if (!w)
        for (int k = 0; k < spec->nitems; k++)
            if (text_w(spec->items[k]) > w)
                w = text_w(spec->items[k]);
    m->title = strdup(spec->title);
    m->w = w;
    m->h = TITLE_H + CELL_STEP * spec->nitems;
    m->nitems = spec->nitems;
    m->items = calloc((size_t)spec->nitems, sizeof *m->items);
    for (int k = 0; k < spec->nitems; k++)
        m->items[k].text = strdup(spec->items[k]);
    m->hilite = -1;
    m->fn = fn;
    m->arg = arg;
    return m;
}

static Window make_window(Window parent, int x, int y, int w, int h,
                          enum xtk_color bg)
{
    XSetWindowAttributes a;

    a.background_pixel = xtk_pixel(bg);
    a.border_pixel = xtk_pixel(XTK_BORDER_IDLE);
    a.event_mask = KeyPressMask | ButtonPressMask | ButtonReleaseMask |
                   EnterWindowMask | LeaveWindowMask | ExposureMask |
                   OwnerGrabButtonMask;
    return XCreateWindow(xtk_dpy(), parent, xtk_s(x), xtk_s(y),
                         (unsigned)xtk_s(w), (unsigned)xtk_s(h),
                         (unsigned)xtk_s(2), CopyFromParent, InputOutput,
                         CopyFromParent,
                         CWBackPixel | CWBorderPixel | CWEventMask, &a);
}

static void show(xtk_menu *m, int x, int y, int default_item)
{
    Display *dpy = xtk_dpy();

    if (active)
        xtk_menu_close(active, false);
    m->x = x;
    m->y = y;
    m->win = make_window(xtk_frame(), x, y, m->w, m->h, XTK_TITLE_BG);
    xtk_surface_init(&m->surf, m->win, m->w, m->h);
    for (int k = 0; k < m->nitems; k++) {
        struct item *it = &m->items[k];
        it->win = make_window(m->win, -2, TITLE_H + CELL_STEP * k,
                              m->w, CELL_H, XTK_BG);
        xtk_surface_init(&it->surf, it->win, m->w, CELL_H);
        XMapWindow(dpy, it->win);
    }
    draw_title(m);
    for (int k = 0; k < m->nitems; k++)
        draw_item(m, k);
    XMapRaised(dpy, m->win);
    active = m;

    if (default_item < 0 || default_item >= m->nitems)
        default_item = 0;
    /* Onto the default item: (w/2 - 4, 4) inside its cell. */
    xtk_warp_frame(x + 2 + m->w / 2 - 4,
                   y + 2 + TITLE_H + CELL_STEP * default_item + 2 + 4);
}

xtk_menu *xtk_menu_open_at(const xtk_menu_spec *spec, int fx, int fy,
                           int anchor_x, int anchor_y,
                           xtk_menu_fn fn, void *arg)
{
    xtk_menu *m = create(spec, fn, arg);

    m->ax = anchor_x;
    m->ay = anchor_y;
    show(m, fx, fy, spec->default_item);
    return m;
}

xtk_menu *xtk_menu_open_anchored(const xtk_menu_spec *spec,
                                 int anchor_x, int anchor_y,
                                 xtk_menu_fn fn, void *arg)
{
    xtk_menu *m = create(spec, fn, arg);
    int outer_w = m->w + 4, outer_h = m->h + 4;
    /*
     * y = anchor - 10 is observed.  The only recorded x placements were
     * clamped against the frame's right edge (Options and Display controls
     * both end at 957), so x = anchor - 10 mirrors y and is unverified for
     * menus opened away from the edge.
     */
    int x = anchor_x - 10, y = anchor_y - 10;

    if (x + outer_w > xtk_frame_w())
        x = xtk_frame_w() - outer_w;
    if (y + outer_h > xtk_frame_h())
        y = xtk_frame_h() - outer_h;
    if (x < 0)
        x = 0;
    if (y < 0)
        y = 0;
    m->ax = anchor_x;
    m->ay = anchor_y;
    show(m, x, y, spec->default_item);
    return m;
}

void xtk_menu_set_item(xtk_menu *m, int item, const char *text)
{
    if (item < 0 || item >= m->nitems)
        return;
    free(m->items[item].text);
    m->items[item].text = strdup(text);
    draw_item(m, item);
}

void xtk_menu_anchor(const xtk_menu *m, int *ax, int *ay)
{
    *ax = m->ax;
    *ay = m->ay;
}

void xtk_menu_close(xtk_menu *m, bool warp_back)
{
    Display *dpy = xtk_dpy();

    if (active == m)
        active = NULL;
    for (int k = 0; k < m->nitems; k++) {
        xtk_surface_free(&m->items[k].surf);
        free(m->items[k].text);
    }
    xtk_surface_free(&m->surf);
    XDestroyWindow(dpy, m->win);
    if (warp_back)
        xtk_warp_frame(m->ax, m->ay);
    free(m->items);
    free(m->title);
    free(m);
}

bool xtk_menu_active(void)
{
    return active != NULL;
}

static int item_of(xtk_menu *m, Window w)
{
    for (int k = 0; k < m->nitems; k++)
        if (m->items[k].win == w)
            return k;
    return -1;
}

bool xtk_menu_handle_event(const XEvent *ev)
{
    xtk_menu *m = active;
    Window w = ev->xany.window;
    int k;

    if (!m)
        return false;
    k = item_of(m, w);
    if (w != m->win && k < 0)
        return false;

    switch (ev->type) {
    case Expose:
        if (ev->xexpose.count == 0) {
            if (w == m->win)
                xtk_surface_present(&m->surf);
            else
                xtk_surface_present(&m->items[k].surf);
        }
        break;
    case EnterNotify:
        if (k >= 0)
            set_hilite(m, k);
        break;
    case LeaveNotify:
        if (k >= 0 && ev->xcrossing.detail != NotifyInferior)
            set_hilite(m, -1);
        /* Leaving the menu itself (not into a cell) closes it. */
        if (w == m->win && ev->xcrossing.detail != NotifyInferior)
            xtk_menu_close(m, false);
        break;
    case ButtonPress:
        if (k >= 0 && m->fn)
            m->fn(m, k, (int)ev->xbutton.button, m->arg);
        break;
    }
    return true;
}
