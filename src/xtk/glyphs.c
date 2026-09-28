/*
 * Bitmaps recovered from xldb.
 */
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

/* Pack a 16x16 character picture into XBM bits, selecting chars in `set`. */
static void pack(const char *const rows[16], const char *set, char bits[32])
{
    for (int y = 0; y < 16; y++) {
        unsigned v = 0;
        for (int px = 0; px < 16; px++) {
            char ch = rows[y][px];
            for (const char *s = set; *s; s++)
                if (ch == *s)
                    v |= 1u << px;
        }
        bits[y * 2] = (char)(v & 0xff);
        bits[y * 2 + 1] = (char)(v >> 8);
    }
}

Cursor xtk_glyph_pointer_cursor(void)
{
    Display *dpy = xtk_dpy();
    char src_bits[32], mask_bits[32];
    Pixmap src, mask;
    XColor fg = { .red = 0xffff, .green = 0xffff, .blue = 0xffff };
    XColor bg = { .red = 0, .green = 0, .blue = 0 };
    Cursor c;

    pack(pointer_rows, "W", src_bits);
    pack(pointer_rows, "W#", mask_bits);
    src = XCreateBitmapFromData(dpy, xtk_root(), src_bits, 16, 16);
    mask = XCreateBitmapFromData(dpy, xtk_root(), mask_bits, 16, 16);
    c = XCreatePixmapCursor(dpy, src, mask, &fg, &bg, 1, 1);
    XFreePixmap(dpy, src);
    XFreePixmap(dpy, mask);
    return c;
}
