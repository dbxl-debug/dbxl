/*
 * Bitmaps recovered from xldb: cursors, the icon and the margin glyphs
 * (DESIGN.md 5.4).
 */
#include <stdlib.h>
#include <string.h>

#include "xtk/xtk.h"
#include "xtk/glyph_bits.h"

/*
 * xldb's pointer: white body (mouseBody), black outline (mouseOutline),
 * hotspot (1,1).  W = source bit (foreground), # = mask only, . = clear.
 */
static const char *const pointer_rows[16] = {
    "##..............",
    "#W#.............",
    "#WW#............",
    "#WWW#...........",
    "#WWWW#..........",
    "#WWWWW#.........",
    "#WWWWWW#........",
    "#WWWWWWW#.......",
    "#WWWWWWWW#......",
    "#WWWWW####......",
    "#WW#WW#.........",
    "#W#.#WW#........",
    "##..#WW#........",
    ".....#WW#.......",
    ".....#WW#.......",
    "......###.......",
};

/*
 * A depth-1 pixmap from a character picture (w x h), selecting the chars
 * in `set`, with every pixel enlarged to scale x scale.
 */
static Pixmap bitmap(const char *const *rows, int w, int h, const char *set,
                     int scale)
{
    int bw = w * scale, bh = h * scale, stride = (bw + 7) / 8;
    char *bits = calloc((size_t)(stride * bh), 1);
    Pixmap pm;

    for (int y = 0; y < bh; y++)
        for (int px = 0; px < bw; px++)
            if (strchr(set, rows[y / scale][px / scale]))
                bits[y * stride + px / 8] |= (char)(1 << (px % 8));
    pm = XCreateBitmapFromData(xtk_dpy(), xtk_root(), bits,
                               (unsigned)bw, (unsigned)bh);
    free(bits);
    return pm;
}

static XColor rgb_of(enum xtk_color c)
{
    XColor x = { .pixel = xtk_pixel(c) };

    XQueryColor(xtk_dpy(), DefaultColormap(xtk_dpy(), DefaultScreen(xtk_dpy())), &x);
    return x;
}

static Cursor make_cursor(const char *const rows[16], const char *src_set,
                          const char *mask_set, int hx, int hy)
{
    Display *dpy = xtk_dpy();
    int scale = xtk_scale();
    Pixmap src = bitmap(rows, 16, 16, src_set, scale);
    Pixmap mask = bitmap(rows, 16, 16, mask_set, scale);
    XColor fg = rgb_of(XTK_POINTER_BODY), bg = rgb_of(XTK_POINTER_OUTLINE);
    Cursor c;

    /* The hotspot is the top-left of the enlarged hotspot pixel. */
    c = XCreatePixmapCursor(dpy, src, mask, &fg, &bg,
                            (unsigned)(hx * scale), (unsigned)(hy * scale));
    XFreePixmap(dpy, src);
    XFreePixmap(dpy, mask);
    return c;
}

Cursor xtk_glyph_pointer_cursor(void)
{
    return make_cursor(pointer_rows, "W", "W#", 1, 1);
}

/* The stopwatch: its mask equals its source, so it is white only. */
Cursor xtk_glyph_busy_cursor(void)
{
    return make_cursor(busy_rows, "#", "#", 7, 2);
}

/* The 64x64 bug icon for WM_HINTS (not scaled: window managers scale). */
Pixmap xtk_glyph_icon(void)
{
    return bitmap(icon_rows, 64, 64, "#", 1);
}

/* Paint the set pixels of a picture in one colour. */
static void paint(Drawable d, const char *const *rows, int w, int h,
                  int ox, int oy, enum xtk_color c)
{
    Display *dpy = xtk_dpy();
    GC gc = xtk_gc();

    XSetForeground(dpy, gc, xtk_pixel(c));
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            if (rows[y][x] == '#')
                XDrawPoint(dpy, d, gc, ox + x, oy + y);
}

/* Paint the pixels set in `outer` but not in `inner`. */
static void paint_ring(Drawable d, const char *const *outer,
                       const char *const *inner, int w, int h,
                       int iox, int ioy, int ox, int oy, enum xtk_color c)
{
    Display *dpy = xtk_dpy();
    GC gc = xtk_gc();

    XSetForeground(dpy, gc, xtk_pixel(c));
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int ix = x - iox, iy = y - ioy;
            bool in = ix >= 0 && iy >= 0 && ix < w && iy < h &&
                      inner[iy][ix] == '#';
            if (outer[y][x] == '#' && !in)
                XDrawPoint(dpy, d, gc, ox + x, oy + y);
        }
}

/*
 * xldb composites each glyph with AndInverted / Or-plane-1 / Or copies
 * (recon pass 10); the resulting colours are painted directly here:
 *   arrow:      ring #000001, interior black        at (2, line_top)
 *   stop sign:  ring #000001, interior #fa1340      at (2, line_top + 1)
 *   disabled:   ring #000001, interior = background
 * In the mono schemes the colours come from the scheme (DESIGN.md 11):
 * filled glyphs use the foreground with a background ring; the disabled
 * stop sign is a foreground ring.
 */
void xtk_glyph_draw(Drawable d, enum xtk_glyph g, int line_top)
{
    bool mono = xtk_mono();

    switch (g) {
    case XTK_GLYPH_ARROW:
        paint_ring(d, arrow_outer, arrow_inner, 32, 16, 0, 0, 2, line_top,
                   mono ? XTK_BG : XTK_GLYPH_OUTLINE);
        paint(d, arrow_inner, 32, 16, 2, line_top,
              mono ? XTK_FG : XTK_GLYPH_FILL);
        break;
    case XTK_GLYPH_STOP:
        paint_ring(d, stop_outer, stop_inner, 11, 11, 0, 0, 2, line_top + 1,
                   mono ? XTK_BG : XTK_GLYPH_OUTLINE);
        paint(d, stop_inner, 11, 11, 2, line_top + 1,
              mono ? XTK_FG : XTK_STOP);
        break;
    case XTK_GLYPH_STOP_DISABLED:
        paint_ring(d, stop_outer, stop_inner, 11, 11, 0, 0, 2, line_top + 1,
                   mono ? XTK_FG : XTK_GLYPH_OUTLINE);
        paint(d, stop_inner, 11, 11, 2, line_top + 1, XTK_BG);
        break;
    }
}
