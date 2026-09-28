/*
 * xtk - dbxl's own X toolkit, drawn directly with Xlib to reproduce xldb's
 * Wintk/Xtk look.  See docs/DESIGN.md sections 4 and 5.
 */
#ifndef DBXL_XTK_H
#define DBXL_XTK_H

#include <stdbool.h>
#include <X11/Xlib.h>

/* Colour roles (DESIGN.md 5.1). */
enum xtk_color {
    XTK_BG,             /* pane background            #4876ff */
    XTK_FG,             /* pane text                  white   */
    XTK_TITLE_BG,       /* title bars, chips          MediumBlue */
    XTK_TITLE_FG,       /* title text                 white   */
    XTK_SELECT_BG,      /* selected line              Cyan    */
    XTK_SELECT_FG,      /* text on selected line      black   */
    XTK_BORDER_IDLE,    /* pane border, idle          black   */
    XTK_BORDER_ACTIVE,  /* pane border, pointer in it white   */
    XTK_THUMB,          /* scroll thumb / arrow fill  #5151fb */
    XTK_OUTLINE,        /* scroll thumb / arrow lines black   */
    XTK_NCOLORS
};

typedef struct xtk_rect {
    int x, y, w, h;
} xtk_rect;

/* Scrollbar flags for a pane. */
enum {
    XTK_SB_NONE       = 0,
    XTK_SB_VERTICAL   = 1 << 0,
    XTK_SB_HORIZONTAL = 1 << 1
};

/* Metrics shared by everything drawn with the text font. */
typedef struct xtk_metrics {
    int char_w;         /* 8 with 8x13 */
    int line_h;         /* 13 with 8x13 */
    int ascent;         /* 11 with 8x13 */
    int title_h;        /* line_h - 1 = 12 */
    int title_base;     /* baseline of title text = ascent - 1 = 10 */
    int border;         /* pane and frame border width = 2 */
    int sb_w;           /* scrollbar interior width = 9 */
} xtk_metrics;

/* display.c */
bool xtk_open(const char *display_name, const char *font_name);
void xtk_close(void);
Display *xtk_dpy(void);
Window xtk_root(void);
unsigned long xtk_pixel(enum xtk_color c);
const xtk_metrics *xtk_metrics_get(void);
GC xtk_gc(void);                        /* scratch GC with the text font */
Pixmap xtk_frame_tile(void);            /* 4x4 stipple, white on bg */
Pixmap xtk_trough_tile(void);           /* 1x1 checkerboard, white on bg */
Cursor xtk_pointer(void);               /* xldb's white arrow pointer */
void xtk_draw_text(Drawable d, enum xtk_color fg, int x, int baseline,
                   const char *s, int len);
void xtk_fill(Drawable d, enum xtk_color c, int x, int y, int w, int h);

/* frame.c */
Window xtk_frame_create(xtk_rect geom, const char *title,
                        const char *icon_name, int argc, char **argv);
Window xtk_frame(void);

/* pane.c */
typedef struct xtk_pane xtk_pane;
xtk_pane *xtk_pane_create(const char *title, xtk_rect geom, int scrollbars);
void xtk_pane_set_title(xtk_pane *p, const char *title);
void xtk_pane_set_lines(xtk_pane *p, const char *const *lines, int n);
void xtk_pane_set_selected(xtk_pane *p, int line);
void xtk_pane_map(xtk_pane *p);
void xtk_pane_unmap(xtk_pane *p);
bool xtk_pane_handle_event(xtk_pane *p, const XEvent *ev);
Window xtk_pane_window(const xtk_pane *p);

/* chip.c: a minimized pane's title-only icon */
typedef struct xtk_chip xtk_chip;
xtk_chip *xtk_chip_create(const char *label, xtk_rect geom);
void xtk_chip_map(xtk_chip *c);
void xtk_chip_unmap(xtk_chip *c);
bool xtk_chip_handle_event(xtk_chip *c, const XEvent *ev);

/* scrollbar.c */
typedef struct xtk_scrollbar xtk_scrollbar;
xtk_scrollbar *xtk_scrollbar_create(Window parent, bool vertical);
void xtk_scrollbar_place(xtk_scrollbar *sb, int parent_w, int parent_h);
void xtk_scrollbar_map(xtk_scrollbar *sb);
bool xtk_scrollbar_handle_event(xtk_scrollbar *sb, const XEvent *ev);

/* glyphs.c */
Cursor xtk_glyph_pointer_cursor(void);

/* loop.c */
typedef bool (*xtk_event_fn)(const XEvent *ev, void *arg);
void xtk_loop_add_handler(xtk_event_fn fn, void *arg);
int xtk_loop_run(void);
void xtk_loop_quit(int status);

#endif
