/*
 * Display connection, colours, font, shared GC and background tiles.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xresource.h>
#include <X11/extensions/Xrender.h>

#include "xtk/xtk.h"

static struct {
    Display *dpy;
    int screen;
    int scale;
    enum xtk_scheme scheme;
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
 * Colour tables per scheme.  The colour scheme uses xldb's literal RGB
 * values (recon pass 9: RoyalBlue1's value although the documentation says
 * "RoyalBlue").  The mono tables are measured from -bw / -wb (passes 7, 8),
 * except the glyph roles, where dbxl deliberately uses the scheme's colours
 * instead of xldb's all-black glyphs (DESIGN.md 11).
 */
#define W 0xffffff
#define K 0x000000
static const unsigned int scheme_rgb[3][XTK_NCOLORS] = {
    [XTK_SCHEME_COLOR] = {
        [XTK_BG] = 0x4876ff,        [XTK_FG] = W,
        [XTK_TITLE_BG] = 0x0000cd,  [XTK_TITLE_FG] = W,
        [XTK_SELECT_BG] = 0x00ffff, [XTK_SELECT_FG] = K,
        [XTK_BORDER_IDLE] = K,      [XTK_BORDER_ACTIVE] = W,
        [XTK_THUMB] = 0x5151fb,     [XTK_OUTLINE] = K,
        [XTK_MENU_HL_BG] = K,       [XTK_MENU_HL_FG] = W,
        [XTK_MENUINFO_FG] = 0xd0d0d0,
        [XTK_DIALOG_BG] = 0x0000cd, [XTK_DIALOG_FG] = W,
        [XTK_FIELD_BG] = 0xadd8e6,  [XTK_FIELD_FG] = K,
        [XTK_CURSOR] = 0xfa1340,    [XTK_STOP] = 0xfa1340,
        [XTK_GLYPH_OUTLINE] = 0x000001, [XTK_GLYPH_FILL] = K,
        [XTK_MARK_BG] = W,          [XTK_MARK_FG] = K,
        [XTK_POINTER_BODY] = W,     [XTK_POINTER_OUTLINE] = K,
    },
    [XTK_SCHEME_BW] = {
        [XTK_BG] = W,               [XTK_FG] = K,
        [XTK_TITLE_BG] = K,         [XTK_TITLE_FG] = W,
        [XTK_SELECT_BG] = K,        [XTK_SELECT_FG] = W,
        [XTK_BORDER_IDLE] = K,      [XTK_BORDER_ACTIVE] = K,
        [XTK_THUMB] = K,            [XTK_OUTLINE] = K,
        [XTK_MENU_HL_BG] = K,       [XTK_MENU_HL_FG] = W,
        [XTK_MENUINFO_FG] = K,
        [XTK_DIALOG_BG] = K,        [XTK_DIALOG_FG] = W,
        [XTK_FIELD_BG] = K,         [XTK_FIELD_FG] = W,
        [XTK_CURSOR] = W,           [XTK_STOP] = K,
        [XTK_GLYPH_OUTLINE] = W,    [XTK_GLYPH_FILL] = K,
        [XTK_MARK_BG] = K,          [XTK_MARK_FG] = W,
        [XTK_POINTER_BODY] = W,     [XTK_POINTER_OUTLINE] = K,
    },
    [XTK_SCHEME_WB] = {
        [XTK_BG] = K,               [XTK_FG] = W,
        [XTK_TITLE_BG] = W,         [XTK_TITLE_FG] = K,
        [XTK_SELECT_BG] = W,        [XTK_SELECT_FG] = K,
        [XTK_BORDER_IDLE] = W,      [XTK_BORDER_ACTIVE] = W,
        [XTK_THUMB] = W,            [XTK_OUTLINE] = W,
        [XTK_MENU_HL_BG] = W,       [XTK_MENU_HL_FG] = K,
        [XTK_MENUINFO_FG] = W,
        [XTK_DIALOG_BG] = W,        [XTK_DIALOG_FG] = K,
        [XTK_FIELD_BG] = W,         [XTK_FIELD_FG] = K,
        [XTK_CURSOR] = K,           [XTK_STOP] = W,
        [XTK_GLYPH_OUTLINE] = K,    [XTK_GLYPH_FILL] = W,
        [XTK_MARK_BG] = W,          [XTK_MARK_FG] = K,
        [XTK_POINTER_BODY] = W,     [XTK_POINTER_OUTLINE] = K,
    },
};
#undef W
#undef K

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

/*
 * A 16x16 logical tile in the screen depth, enlarged by `scale`: white
 * where pattern(px, py), else bg.
 */
static Pixmap make_tile(bool (*pattern)(int, int), int scale)
{
    unsigned n = (unsigned)(16 * scale);
    Pixmap pm = XCreatePixmap(x.dpy, x.root, n, n,
                              (unsigned)DefaultDepth(x.dpy, x.screen));
    XGCValues v;
    GC gc;

    v.foreground = x.pixel[XTK_BG];
    gc = XCreateGC(x.dpy, pm, GCForeground, &v);
    XFillRectangle(x.dpy, pm, gc, 0, 0, n, n);
    XSetForeground(x.dpy, gc, x.pixel[XTK_FG]);
    for (int py = 0; py < 16; py++)
        for (int px = 0; px < 16; px++)
            if (pattern(px, py))
                XFillRectangle(x.dpy, pm, gc, px * scale, py * scale,
                               (unsigned)scale, (unsigned)scale);
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

/* Dots per inch: the Xft.dpi resource if set, else the screen's size. */
static double screen_dpi(void)
{
    const char *rm = XResourceManagerString(x.dpy);
    int mm = DisplayWidthMM(x.dpy, x.screen);

    if (rm) {
        XrmDatabase db;
        XrmInitialize();
        db = XrmGetStringDatabase(rm);
        char *type;
        XrmValue val;
        double dpi = 0;

        if (XrmGetResource(db, "Xft.dpi", "Xft.Dpi", &type, &val) && val.addr)
            dpi = atof(val.addr);
        XrmDestroyDatabase(db);
        if (dpi > 0)
            return dpi;
    }
    return mm > 0 ? DisplayWidth(x.dpy, x.screen) * 25.4 / mm : 96.0;
}

/* scale 0 = auto: round(dpi / 96), then shrink until the frame fits. */
static int choose_scale(int scale, int fit_w, int fit_h)
{
    int sw = DisplayWidth(x.dpy, x.screen);
    int sh = DisplayHeight(x.dpy, x.screen);

    if (scale == 0) {
        scale = (int)(screen_dpi() / 96.0 + 0.5);
        if (scale > 4)
            scale = 4;
        while (scale > 1 && (fit_w * scale > sw || fit_h * scale > sh))
            scale--;
    }
    if (scale < 1)
        scale = 1;
    if (scale > 1) {
        int ev, err;
        if (!XRenderQueryExtension(x.dpy, &ev, &err)) {
            fprintf(stderr, "dbxl: the X server lacks the RENDER extension; "
                            "using -scale 1\n");
            scale = 1;
        }
    }
    return scale;
}

bool xtk_open(const char *display_name)
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
    return true;
}

/* A colour spec from a resource or option; fall back to the default. */
static unsigned long alloc_spec(const char *spec, unsigned int dflt)
{
    XColor c;

    if (spec && XParseColor(x.dpy, x.cmap, spec, &c) &&
        XAllocColor(x.dpy, x.cmap, &c))
        return c.pixel;
    if (spec)
        fprintf(stderr, "dbxl: unknown colour %s\n", spec);
    return alloc_rgb(dflt);
}

bool xtk_setup(const xtk_config *cfg)
{
    const char *font_name = cfg->font ? cfg->font : "8x13";

    x.scale = choose_scale(cfg->scale, cfg->fit_w, cfg->fit_h);
    x.scheme = cfg->scheme;
    for (int i = 0; i < XTK_NCOLORS; i++) {
        unsigned int dflt = scheme_rgb[x.scheme][i];
        /* Colour resources are ignored in the mono schemes, as in xldb. */
        x.pixel[i] = x.scheme == XTK_SCHEME_COLOR
                   ? alloc_spec(cfg->color[i], dflt) : alloc_rgb(dflt);
    }

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

    x.frame_tile = make_tile(frame_pattern, x.scale);
    x.trough_tile = make_tile(trough_pattern, 1);
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
int xtk_scale(void) { return x.scale; }
bool xtk_mono(void) { return x.scheme != XTK_SCHEME_COLOR; }
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

void xtk_fill_tiled(Drawable d, Pixmap tile, int fx, int fy, int w, int h)
{
    if (w <= 0 || h <= 0)
        return;
    XSetTile(x.dpy, x.gc, tile);
    XSetTSOrigin(x.dpy, x.gc, 0, 0);
    XSetFillStyle(x.dpy, x.gc, FillTiled);
    XFillRectangle(x.dpy, d, x.gc, fx, fy, (unsigned)w, (unsigned)h);
    XSetFillStyle(x.dpy, x.gc, FillSolid);
}
