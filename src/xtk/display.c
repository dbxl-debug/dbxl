/*
 * Display connection, colours, font, shared GC and background tiles.
 */
#include <stdio.h>
#include <string.h>

#include "xtk/xtk.h"

static struct {
    Display *dpy;
    int screen;
    Window root;
    Colormap cmap;
    unsigned long pixel[XTK_NCOLORS];
    XFontStruct *font;
    xtk_metrics m;
    GC gc;
    Pixmap frame_tile;
    Pixmap trough_tile;
    Cursor pointer;
} x;

/*
 * Default colours.  xldb allocates most of these by literal RGB rather than
 * by name (recon pass 9), so RoyalBlue1's value is used even though the
 * documentation says "RoyalBlue".
 */
static const unsigned int default_rgb[XTK_NCOLORS] = {
    [XTK_BG]            = 0x4876ff,
    [XTK_FG]            = 0xffffff,
    [XTK_TITLE_BG]      = 0x0000cd,   /* MediumBlue */
    [XTK_TITLE_FG]      = 0xffffff,
    [XTK_SELECT_BG]     = 0x00ffff,   /* Cyan */
    [XTK_SELECT_FG]     = 0x000000,
    [XTK_BORDER_IDLE]   = 0x000000,
    [XTK_BORDER_ACTIVE] = 0xffffff,
    [XTK_THUMB]         = 0x5151fb,
    [XTK_OUTLINE]       = 0x000000,
};

static unsigned long alloc_rgb(unsigned int rgb)
{
    XColor c;

    memset(&c, 0, sizeof c);
    c.red   = (unsigned short)(((rgb >> 16) & 0xff) * 0x101);
    c.green = (unsigned short)(((rgb >> 8) & 0xff) * 0x101);
    c.blue  = (unsigned short)((rgb & 0xff) * 0x101);
    c.flags = DoRed | DoGreen | DoBlue;
    if (!XAllocColor(x.dpy, x.cmap, &c))
        return (rgb & 0x808080) ? WhitePixel(x.dpy, x.screen)
                                : BlackPixel(x.dpy, x.screen);
    return c.pixel;
}

/* A 16x16 tile in the screen depth: white where pattern(px, py), else bg. */
static Pixmap make_tile(bool (*pattern)(int, int))
{
    Pixmap pm = XCreatePixmap(x.dpy, x.root, 16, 16,
                              (unsigned)DefaultDepth(x.dpy, x.screen));
    XGCValues v;
    GC gc;

    v.foreground = x.pixel[XTK_BG];
    gc = XCreateGC(x.dpy, pm, GCForeground, &v);
    XFillRectangle(x.dpy, pm, gc, 0, 0, 16, 16);
    XSetForeground(x.dpy, gc, x.pixel[XTK_FG]);
    for (int py = 0; py < 16; py++)
        for (int px = 0; px < 16; px++)
            if (pattern(px, py))
                XDrawPoint(x.dpy, pm, gc, px, py);
    XFreeGC(x.dpy, gc);
    return pm;
}

/*
 * Frame stipple, phase relative to the frame's inside origin (recon pass 9):
 *   W b W b
 *   b b b W
 *   W b W b
 *   b W b b
 */
static bool frame_pattern(int px, int py)
{
    switch (py % 4) {
    case 0:
    case 2:  return px % 2 == 0;
    case 1:  return px % 4 == 3;
    default: return px % 4 == 1;
    }
}

/* Scrollbar trough: 1x1 checkerboard, white at (0,0). */
static bool trough_pattern(int px, int py)
{
    return (px + py) % 2 == 0;
}

bool xtk_open(const char *display_name, const char *font_name)
{
    x.dpy = XOpenDisplay(display_name);
    if (!x.dpy) {
        fprintf(stderr, "dbxl: cannot open display %s\n",
                display_name ? display_name : XDisplayName(NULL));
        return false;
    }
    x.screen = DefaultScreen(x.dpy);
    x.root = RootWindow(x.dpy, x.screen);
    x.cmap = DefaultColormap(x.dpy, x.screen);

    for (int i = 0; i < XTK_NCOLORS; i++)
        x.pixel[i] = alloc_rgb(default_rgb[i]);

    x.font = XLoadQueryFont(x.dpy, font_name);
    if (!x.font) {
        fprintf(stderr, "Unable to find font '%s' - using '%s' instead.\n",
                font_name, "8x13");
        x.font = XLoadQueryFont(x.dpy, "8x13");
    }
    if (!x.font) {
        fprintf(stderr, "Unable to find font %s.\n", "8x13");
        XCloseDisplay(x.dpy);
        x.dpy = NULL;
        return false;
    }

    x.m.char_w = x.font->max_bounds.width;
    x.m.ascent = x.font->ascent;
    x.m.line_h = x.font->ascent + x.font->descent;
    x.m.title_h = x.m.line_h - 1;
    x.m.title_base = x.m.ascent - 1;
    x.m.border = 2;
    x.m.sb_w = 9;

    XGCValues v;
    v.font = x.font->fid;
    v.graphics_exposures = False;
    x.gc = XCreateGC(x.dpy, x.root, GCFont | GCGraphicsExposures, &v);

    x.frame_tile = make_tile(frame_pattern);
    x.trough_tile = make_tile(trough_pattern);
    x.pointer = xtk_glyph_pointer_cursor();
    return true;
}

void xtk_close(void)
{
    if (!x.dpy)
        return;
    XFreeCursor(x.dpy, x.pointer);
    XFreePixmap(x.dpy, x.frame_tile);
    XFreePixmap(x.dpy, x.trough_tile);
    XFreeGC(x.dpy, x.gc);
    XFreeFont(x.dpy, x.font);
    XCloseDisplay(x.dpy);
    x.dpy = NULL;
}

Display *xtk_dpy(void) { return x.dpy; }
Window xtk_root(void) { return x.root; }
unsigned long xtk_pixel(enum xtk_color c) { return x.pixel[c]; }
const xtk_metrics *xtk_metrics_get(void) { return &x.m; }
GC xtk_gc(void) { return x.gc; }
Pixmap xtk_frame_tile(void) { return x.frame_tile; }
Pixmap xtk_trough_tile(void) { return x.trough_tile; }
Cursor xtk_pointer(void) { return x.pointer; }

void xtk_draw_text(Drawable d, enum xtk_color fg, int tx, int baseline,
                   const char *s, int len)
{
    XSetForeground(x.dpy, x.gc, x.pixel[fg]);
    XDrawString(x.dpy, d, x.gc, tx, baseline, s, len);
}

void xtk_fill(Drawable d, enum xtk_color c, int fx, int fy, int w, int h)
{
    if (w <= 0 || h <= 0)
        return;
    XSetForeground(x.dpy, x.gc, x.pixel[c]);
    XFillRectangle(x.dpy, d, x.gc, fx, fy, (unsigned)w, (unsigned)h);
}
