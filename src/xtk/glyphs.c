/*
 * Bitmaps recovered from xldb.
 */
#include <stdlib.h>

#include "xtk/xtk.h"

/*
 * xldb's pointer: white body (mouseBody), black outline (mouseOutline),
 * hotspot (1,1).  Captured with XFixes in recon pass 6.
 *   W = source bit (foreground), # = mask only (background), . = clear
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
 * Build an XBM bitmap from a 16x16 character picture, selecting the chars
 * in `set`, with every pixel enlarged to scale x scale.
 */
static Pixmap bitmap(const char *const rows[16], const char *set, int scale)
{
    int n = 16 * scale, stride = (n + 7) / 8;
    char *bits = calloc((size_t)(stride * n), 1);
    Pixmap pm;

    for (int y = 0; y < n; y++)
        for (int px = 0; px < n; px++) {
            char ch = rows[y / scale][px / scale];
            for (const char *s = set; *s; s++)
                if (ch == *s)
                    bits[y * stride + px / 8] |= (char)(1 << (px % 8));
        }
    pm = XCreateBitmapFromData(xtk_dpy(), xtk_root(), bits,
                               (unsigned)n, (unsigned)n);
    free(bits);
    return pm;
}

Cursor xtk_glyph_pointer_cursor(void)
{
    Display *dpy = xtk_dpy();
    int scale = xtk_scale();
    Pixmap src = bitmap(pointer_rows, "W", scale);
    Pixmap mask = bitmap(pointer_rows, "W#", scale);
    XColor fg = { .red = 0xffff, .green = 0xffff, .blue = 0xffff };
    XColor bg = { .red = 0, .green = 0, .blue = 0 };
    Cursor c;

    /* The hotspot is the top-left of the enlarged (1,1) pixel. */
    c = XCreatePixmapCursor(dpy, src, mask, &fg, &bg,
                            (unsigned)scale, (unsigned)scale);
    XFreePixmap(dpy, src);
    XFreePixmap(dpy, mask);
    return c;
}
