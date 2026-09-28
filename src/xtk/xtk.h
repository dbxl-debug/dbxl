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
    XTK_MENU_HL_BG,     /* menu item under pointer    black   */
    XTK_MENU_HL_FG,     /*                            white   */
    XTK_MENUINFO_FG,    /* non-selectable menu text   #d0d0d0 */
    XTK_DIALOG_BG,      /* dialogs, Messages bar      MediumBlue */
    XTK_DIALOG_FG,      /*                            white   */
    XTK_FIELD_BG,       /* text entry fields          LightBlue */
    XTK_FIELD_FG,       /*                            black   */
    XTK_CURSOR,         /* input cursor box           #fa1340 */
    XTK_STOP,           /* stop sign fill             #fa1340 */
    XTK_GLYPH_OUTLINE,  /* glyph ring                 #000001 */
    XTK_GLYPH_FILL,     /* arrow interior             black   */
    XTK_MARK_BG,        /* emphasized text            (unobserved) */
    XTK_MARK_FG,
    XTK_POINTER_BODY,   /* mouseBody                  white   */
    XTK_POINTER_OUTLINE,/* mouseOutline               black   */
    XTK_NCOLORS
};

enum xtk_scheme {
    XTK_SCHEME_COLOR,   /* xldb's default colours */
    XTK_SCHEME_BW,      /* -bw: black on white */
    XTK_SCHEME_WB       /* -wb: white on black */
};

typedef struct xtk_point {
    int x, y;
} xtk_point;

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

/*
 * display.c
 *
 * Everything is laid out and drawn in xldb's pixel units ("logical"
 * pixels) and shown enlarged by an integer scale factor (1-4) so it stays
 * readable on high resolution screens.  scale 0 = auto: from the screen's
 * DPI, reduced until a fit_w x fit_h frame fits on the screen.
 */
typedef struct xtk_config {
    const char *font;
    int scale;
    int fit_w, fit_h;
    enum xtk_scheme scheme;
    const char *color[XTK_NCOLORS];     /* overrides (colour scheme only) */
} xtk_config;

bool xtk_open(const char *display_name);
bool xtk_setup(const xtk_config *cfg);
bool xtk_mono(void);
void xtk_close(void);
int xtk_scale(void);
static inline int xtk_s(int logical) { return logical * xtk_scale(); }
Display *xtk_dpy(void);
Window xtk_root(void);
unsigned long xtk_pixel(enum xtk_color c);
const xtk_metrics *xtk_metrics_get(void);
GC xtk_gc(void);                        /* scratch GC with the text font */
Pixmap xtk_frame_tile(void);            /* 4x4 stipple, white on bg; scaled */
Pixmap xtk_trough_tile(void);           /* 1x1 checkerboard, white on bg; 1x */
Cursor xtk_pointer(void);               /* xldb's white arrow pointer */
void xtk_draw_text(Drawable d, enum xtk_color fg, int x, int baseline,
                   const char *s, int len);
void xtk_fill(Drawable d, enum xtk_color c, int x, int y, int w, int h);
void xtk_fill_tiled(Drawable d, Pixmap tile, int x, int y, int w, int h);

/*
 * surface.c: a window's 1x backing pixmap.  Draw into `pm` in logical
 * pixels, then present to copy it to the window enlarged by the scale.
 */
typedef struct xtk_surface {
    Window win;
    Pixmap pm;
    int w, h;                    /* logical size */
    unsigned long src_pict;      /* XRender Picture, 0 at scale 1 */
    unsigned long dst_pict;
} xtk_surface;
void xtk_surface_init(xtk_surface *s, Window win, int w, int h);
void xtk_surface_resize(xtk_surface *s, int w, int h);
void xtk_surface_present(xtk_surface *s);
void xtk_surface_free(xtk_surface *s);

/* frame.c */
Window xtk_frame_create(xtk_rect geom, const char *title,
                        const char *icon_name, int argc, char **argv);
Window xtk_frame(void);
int xtk_frame_w(void);                   /* logical inside size */
int xtk_frame_h(void);
void xtk_frame_busy(bool busy);          /* stopwatch pointer while working */

/* Convert a root position in physical pixels to logical frame coordinates. */
void xtk_root_to_frame(int rx, int ry, int *fx, int *fy);
/* Warp the pointer to a logical frame position. */
void xtk_warp_frame(int fx, int fy);
/* The pointer's logical frame position. */
void xtk_pointer_frame(int *fx, int *fy);

/*
 * pane.c
 *
 * Panes report user input to their owner through one callback.  Positions
 * are logical: x, y relative to the pane's inside origin; fx, fy in frame
 * coordinates.
 */
typedef struct xtk_pane xtk_pane;

enum xtk_pane_event_type {
    XTK_PANE_TITLE_CLICK,    /* button on the title bar */
    XTK_PANE_CLICK,          /* button in the content area: row, col */
    XTK_PANE_KEY,            /* key press; keysym, text */
    XTK_PANE_STRIP_DONE,     /* inline strip Return: text (":12") */
};

typedef struct xtk_pane_event {
    enum xtk_pane_event_type type;
    int button;
    int x, y, fx, fy;
    int row, col;            /* content row/column (0-based) */
    int line;                /* row + the pane's top line */
    unsigned long keysym;
    unsigned int state;      /* modifier mask of key/button events */
    const char *text;
} xtk_pane_event;

typedef void (*xtk_pane_fn)(xtk_pane *p, const xtk_pane_event *ev, void *arg);

xtk_pane *xtk_pane_create(const char *title, xtk_rect geom, int scrollbars);
void xtk_pane_set_handler(xtk_pane *p, xtk_pane_fn fn, void *arg);
void xtk_pane_set_title(xtk_pane *p, const char *title);
void xtk_pane_set_tag(xtk_pane *p, const char *tag);     /* NULL = none */
void xtk_pane_set_lines(xtk_pane *p, const char *const *lines, int n);
int xtk_pane_nlines(const xtk_pane *p);
const char *xtk_pane_line(const xtk_pane *p, int i);
int xtk_pane_rows(const xtk_pane *p);                    /* visible rows */
void xtk_pane_set_selected(xtk_pane *p, int line);       /* -1 = none */
/* Scrolling: the first visible line (0-based). */
void xtk_pane_set_top(xtk_pane *p, int top);
int xtk_pane_top(const xtk_pane *p);
/* Margin glyphs per line: stop sign or disabled stop sign, then the arrow. */
enum { XTK_MARK_STOP = 1, XTK_MARK_STOP_DISABLED = 2, XTK_MARK_ARROW = 4 };
void xtk_pane_set_marks(xtk_pane *p, const unsigned char *marks, int n);
/* Per-line text roles: XTK_ROLE_INFO draws a line in the menuInfo colour. */
enum { XTK_ROLE_NORMAL = 0, XTK_ROLE_INFO = 1 };
void xtk_pane_set_line_roles(xtk_pane *p, const unsigned char *roles, int n);
void xtk_pane_map(xtk_pane *p);
void xtk_pane_unmap(xtk_pane *p);
bool xtk_pane_mapped(const xtk_pane *p);
void xtk_pane_raise(xtk_pane *p);
void xtk_pane_lower(xtk_pane *p);
xtk_rect xtk_pane_geometry(const xtk_pane *p);          /* excl. border */
void xtk_pane_set_geometry(xtk_pane *p, xtk_rect g);
int xtk_pane_scrollbars(const xtk_pane *p);
void xtk_pane_set_scrollbars(xtk_pane *p, int flags);
void xtk_pane_set_autoraise(bool on);
bool xtk_pane_autoraise(void);
/* Interactive move/resize from the Window Control menu (see gesture.c). */
void xtk_pane_begin_move(xtk_pane *p);
void xtk_pane_begin_size(xtk_pane *p);
/* Inline input strip (":", "/", ...) at the pointer's row. */
void xtk_pane_strip_open(xtk_pane *p, int fy, const char *initial);
bool xtk_pane_handle_event(xtk_pane *p, const XEvent *ev);
Window xtk_pane_window(const xtk_pane *p);

/* chip.c: a minimized pane's title-only icon */
typedef struct xtk_chip xtk_chip;
typedef void (*xtk_chip_fn)(xtk_chip *c, int button, void *arg);
xtk_chip *xtk_chip_create(const char *label, xtk_rect geom);
void xtk_chip_set_handler(xtk_chip *c, xtk_chip_fn fn, void *arg);
void xtk_chip_set_geometry(xtk_chip *c, xtk_rect geom);
void xtk_chip_map(xtk_chip *c);
void xtk_chip_unmap(xtk_chip *c);
bool xtk_chip_handle_event(xtk_chip *c, const XEvent *ev);

/* scrollbar.c */
typedef struct xtk_scrollbar xtk_scrollbar;
xtk_scrollbar *xtk_scrollbar_create(Window parent, bool vertical);
void xtk_scrollbar_place(xtk_scrollbar *sb, int parent_w, int parent_h);
void xtk_scrollbar_map(xtk_scrollbar *sb);
void xtk_scrollbar_unmap(xtk_scrollbar *sb);
/* Arrow clicks: dir -1 (up/left) or +1 (down/right). */
typedef void (*xtk_scrollbar_fn)(xtk_scrollbar *sb, int dir, void *arg);
void xtk_scrollbar_set_handler(xtk_scrollbar *sb, xtk_scrollbar_fn fn,
                               void *arg);
/* The thumb's leading edge in bar coordinates (13 at the start). */
void xtk_scrollbar_set_thumb(xtk_scrollbar *sb, int pos);
bool xtk_scrollbar_handle_event(xtk_scrollbar *sb, const XEvent *ev);

/*
 * menu.c: xldb's "cell" menus (Window Control, Options, ...).  A menu is a
 * frame child: a title row, then one bordered cell per item.  Opening warps
 * the pointer onto the default item; the menu closes when the pointer
 * leaves it.  The callback decides whether a selection closes the menu.
 */
typedef struct xtk_menu xtk_menu;
typedef void (*xtk_menu_fn)(xtk_menu *m, int item, int button, void *arg);

typedef struct xtk_menu_spec {
    const char *title;
    const char *const *items;    /* item text, drawn centred */
    int nitems;
    int width;                   /* 0: widest item */
    int default_item;
} xtk_menu_spec;

/* Place at a frame position (outer corner). */
xtk_menu *xtk_menu_open_at(const xtk_menu_spec *spec, int fx, int fy,
                           int anchor_x, int anchor_y,
                           xtk_menu_fn fn, void *arg);
/* Place centred on an anchor (the click point), clamped to the frame. */
xtk_menu *xtk_menu_open_anchored(const xtk_menu_spec *spec,
                                 int anchor_x, int anchor_y,
                                 xtk_menu_fn fn, void *arg);
void xtk_menu_set_item(xtk_menu *m, int item, const char *text);
/* Close; warp_back returns the pointer to the anchor. */
void xtk_menu_close(xtk_menu *m, bool warp_back);
void xtk_menu_anchor(const xtk_menu *m, int *ax, int *ay);
bool xtk_menu_handle_event(const XEvent *ev);
bool xtk_menu_active(void);

/*
 * dialog.c: xldb's dialogs.  A confirm dialog (400x60, centred) has a
 * message and two buttons; a prompt dialog (frame width, top at the
 * frame's middle) adds a text field.  The callback gets the button index
 * (0 = first, e.g. yes/proceed; 1 = second) and the field text.
 */
typedef void (*xtk_dialog_fn)(int button, const char *text, void *arg);
void xtk_dialog_confirm(const char *message, const char *b1, const char *b2,
                        int anchor_x, int anchor_y,
                        xtk_dialog_fn fn, void *arg);
void xtk_dialog_prompt(const char *message, const char *initial,
                       const char *b1, const char *b2,
                       int anchor_x, int anchor_y,
                       xtk_dialog_fn fn, void *arg);
/* The compact prompt: 400 x 60 with its outer corner at frame (x, y). */
void xtk_dialog_prompt_at(const char *message, const char *initial,
                          const char *b1, const char *b2, int x, int y,
                          int anchor_x, int anchor_y,
                          xtk_dialog_fn fn, void *arg);
bool xtk_dialog_handle_event(const XEvent *ev);
bool xtk_dialog_active(void);

/*
 * lineedit.c: the single-line editor behind dialog fields and the inline
 * strip: LightBlue field, black text, red 8x12 cursor box.
 */
typedef struct xtk_lineedit {
    char text[256];
    int len, cursor;
} xtk_lineedit;
void xtk_lineedit_set(xtk_lineedit *le, const char *text);
/* Returns 1 on Return, -1 on Escape, 0 otherwise. */
int xtk_lineedit_key(xtk_lineedit *le, const XKeyEvent *kev);
void xtk_lineedit_draw(const xtk_lineedit *le, Drawable d, int w, int h);

/* gesture.c: rubber-band outlines and interactive move/resize. */
void xtk_rubber_draw(xtk_rect outer);                 /* XOR, frame coords */
bool xtk_gesture_active(void);
bool xtk_gesture_handle_event(const XEvent *ev);

/* glyphs.c */
Cursor xtk_glyph_pointer_cursor(void);
Cursor xtk_glyph_busy_cursor(void);
Pixmap xtk_glyph_icon(void);
enum xtk_glyph { XTK_GLYPH_ARROW, XTK_GLYPH_STOP, XTK_GLYPH_STOP_DISABLED };
/* Draw a margin glyph for the content row whose top is line_top. */
void xtk_glyph_draw(Drawable d, enum xtk_glyph g, int line_top);

/* loop.c */
typedef bool (*xtk_event_fn)(const XEvent *ev, void *arg);
void xtk_loop_add_handler(xtk_event_fn fn, void *arg);
typedef void (*xtk_fd_fn)(int fd, void *arg);
/* Watch a descriptor: fn runs when it is readable (or hung up). */
void xtk_loop_add_fd(int fd, xtk_fd_fn fn, void *arg);
void xtk_loop_remove_fd(int fd);
int xtk_loop_run(void);
void xtk_loop_quit(int status);

#endif
