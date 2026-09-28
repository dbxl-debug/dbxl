/*
 * Values as xldb shows them (DESIGN.md 6, recon passes 4 and 13): the
 * display state of every value (style, detail level, saved style), kept by
 * key so it outlives stops and restarts, and the renderer that turns value
 * trees into pane lines plus the spans a click is resolved against.
 *
 * Keys are "<scope>:<path>": the scope names where the variable lives
 * ("L:main" for main's locals, "G:rich.c" for a file's globals) and the
 * path the node inside it ("sp", "sp*.name[3]").
 */
#ifndef DBXL_VALUE_H
#define DBXL_VALUE_H

#include <stdbool.h>

#include "backend/backend.h"

enum dbxl_style {
    STYLE_DEFAULT,               /* the kind's own */
    STYLE_CHARACTER, STYLE_SIGNED, STYLE_UNSIGNED, STYLE_HEX, STYLE_ADDRESS,
    STYLE_TYPE, STYLE_SIZE, STYLE_STRING, STYLE_ARRAY, STYLE_POINTER,
    STYLE_DECIMAL, STYLE_SCIENTIFIC,
};

typedef struct dbxl_vstate {
    enum dbxl_style style;
    int detail;                  /* 1 collapsed, 2 inline, 3 vertical... */
    bool stepped;                /* more/less/flatten used on it */
    bool flat;                   /* "flatten": horizontal, pointers not followed */
    bool monitored;
    bool has_range;              /* Select subrange: elements lo..hi */
    long lo, hi;
    char cast[128];              /* Cast: the pointed-to type, "" = none */
} dbxl_vstate;

/* The state for key, created (default) when create is set. */
dbxl_vstate *dbxl_vstate_get(const char *key, bool create);

/* Call fn for every state whose key starts with prefix. */
void dbxl_vstate_foreach(const char *prefix,
                         void (*fn)(const char *key, dbxl_vstate *s, void *arg),
                         void *arg);

/* A clickable element: columns [col0, col1) of a line. */
typedef struct dbxl_span {
    int line, col0, col1;
    const dbg_value *v;
    char key[600];
} dbxl_span;

typedef struct dbxl_render {
    char **lines;
    int nlines, cap;
    dbxl_span *spans;
    int nspans, scap;
    char **needs;                /* pointer paths still to fetch ("sp*") */
    int nneeds;
    int ptrsize;
} dbxl_render;

void dbxl_render_init(dbxl_render *r, int ptrsize);
void dbxl_render_free(dbxl_render *r);
void dbxl_render_text(dbxl_render *r, const char *text);
/* "name: value" lines for a variable in scope ("L:main", "G:rich.c"). */
void dbxl_render_var(dbxl_render *r, const char *scope, const dbg_value *v);
/* The same with its own label ("RAX  : " for registers). */
void dbxl_render_var_label(dbxl_render *r, const char *scope,
                           const char *label, const dbg_value *v);
/* The innermost span at (line, col), or NULL. */
const dbxl_span *dbxl_render_hit(const dbxl_render *r, int line, int col);

/* A scalar or pointer as shown (for Edit's initial text). */
void dbxl_format_value(const dbg_value *v, const char *key, int ptrsize,
                       char *buf, int size);
/* Floating point in xldb's styles; digits 6 (float) or 15 (double). */
void dbxl_format_float(double d, int digits, bool decimal, char *buf, int size);
/* The style a kind shows by default. */
enum dbxl_style dbxl_default_style(enum dbg_kind kind);

#endif
