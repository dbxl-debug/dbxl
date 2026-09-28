/*
 * Value display (DESIGN.md 6; recon passes 4 and 13).
 *
 * Detail levels: 1 collapsed (`ptr`, `shape{}`, `[]`), 2 inline
 * (`->shape{}` for a pointer, `{ v v }`, `[ v v ]`), 3 vertical (one
 * `field: value` or `[ i]: value` per line; for a pointer, its target
 * inline), 4 a pointer's target vertical.  A child is shown at least one
 * level below its parent (max(own, parent - 1)), so a vertical array's
 * struct elements show inline.  "flatten" (xldb's help: "show the members
 * horizontally") adds a level like "more" but keeps the value and everything
 * inside it on one line, and doesn't follow pointers.  Inline arrays break after every 10 elements,
 * the continuation indented to the first element.
 */
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/value.h"

/* ---- state ------------------------------------------------------------ */

static struct entry {
    char *key;
    dbxl_vstate st;
} *entries;
static int nentries, centries;

dbxl_vstate *dbxl_vstate_get(const char *key, bool create)
{
    for (int i = 0; i < nentries; i++)
        if (strcmp(entries[i].key, key) == 0)
            return &entries[i].st;
    if (!create)
        return NULL;
    if (nentries == centries) {
        centries = centries ? centries * 2 : 64;
        entries = realloc(entries, (size_t)centries * sizeof *entries);
    }
    entries[nentries].key = strdup(key);
    memset(&entries[nentries].st, 0, sizeof entries[nentries].st);
    entries[nentries].st.detail = 1;
    return &entries[nentries++].st;
}

static int own_detail(const char *key)
{
    dbxl_vstate *s = dbxl_vstate_get(key, false);

    return s ? s->detail : 1;
}

static enum dbxl_style own_style(const char *key)
{
    dbxl_vstate *s = dbxl_vstate_get(key, false);

    return s ? s->style : STYLE_DEFAULT;
}

enum dbxl_style dbxl_default_style(enum dbg_kind kind)
{
    switch (kind) {
    case DBG_K_INT: case DBG_K_BOOL: return STYLE_SIGNED;
    case DBG_K_UINT: case DBG_K_UCHAR: return STYLE_UNSIGNED;
    case DBG_K_CHAR: return STYLE_CHARACTER;
    case DBG_K_FLOAT: return STYLE_SCIENTIFIC;
    case DBG_K_POINTER: return STYLE_POINTER;
    default: return STYLE_DEFAULT;
    }
}

/* ---- scalars ----------------------------------------------------------- */

static uint64_t mask(int size)
{
    return size >= 8 || size <= 0 ? UINT64_MAX : (UINT64_C(1) << (size * 8)) - 1;
}

static void char_literal(unsigned c, char *buf, int size, char quote)
{
    static const char esc[] = { '\a', 'a', '\b', 'b', '\f', 'f', '\n', 'n',
                                '\r', 'r', '\t', 't', '\v', 'v', 0 };
    int n = 0;

    for (int i = 0; esc[i]; i += 2)
        if (c == (unsigned char)esc[i]) {
            snprintf(buf, (size_t)size, "\\%c", esc[i + 1]);
            return;
        }
    if (c == 0)
        n = snprintf(buf, (size_t)size, "\\0");
    else if (c == '\\' || c == (unsigned char)quote)
        n = snprintf(buf, (size_t)size, "\\%c", c);
    else if (c >= 32 && c < 127)
        n = snprintf(buf, (size_t)size, "%c", c);
    else
        n = snprintf(buf, (size_t)size, "\\%03o", c);
    (void)n;
}

/*
 * xldb's floating point: `digits` significant digits, trailing zeros
 * dropped but at least one decimal.  Scientific: `1.5e+0`, `1.23457e+6`;
 * decimal: fixed for exponents -5..6 (`0.00001`, `1234570.0`), otherwise
 * scientific.  Zero is `0.0`.
 */
void dbxl_format_float(double d, int digits, bool decimal, char *buf, int size)
{
    char tmp[64], mant[32];
    const char *e;
    int exp, nd, n = 0;
    bool neg;

    if (isnan(d)) {
        snprintf(buf, (size_t)size, "NaN");
        return;
    }
    if (isinf(d)) {
        snprintf(buf, (size_t)size, d < 0 ? "-Inf" : "Inf");
        return;
    }
    if (d == 0) {
        snprintf(buf, (size_t)size, "0.0");
        return;
    }
    neg = d < 0;
    snprintf(tmp, sizeof tmp, "%.*e", digits - 1, fabs(d));
    e = strchr(tmp, 'e');
    exp = atoi(e + 1);
    /* The significant digits, without the point. */
    for (const char *p = tmp; p < e; p++)
        if (*p != '.')
            mant[n++] = *p;
    mant[n] = '\0';
    while (n > 1 && mant[n - 1] == '0')
        mant[--n] = '\0';
    nd = n;

    if (decimal && exp >= -5 && exp <= 6) {
        char out[64];
        int k = 0;

        if (neg)
            out[k++] = '-';
        if (exp >= 0) {
            for (int i = 0; i <= exp; i++)
                out[k++] = i < nd ? mant[i] : '0';
            out[k++] = '.';
            if (exp + 1 >= nd)
                out[k++] = '0';
            for (int i = exp + 1; i < nd; i++)
                out[k++] = mant[i];
        } else {
            out[k++] = '0';
            out[k++] = '.';
            for (int i = 0; i < -exp - 1; i++)
                out[k++] = '0';
            for (int i = 0; i < nd; i++)
                out[k++] = mant[i];
        }
        out[k] = '\0';
        snprintf(buf, (size_t)size, "%s", out);
        return;
    }
    snprintf(buf, (size_t)size, "%s%c.%s%se%c%d", neg ? "-" : "", mant[0],
             nd > 1 ? mant + 1 : "0", "", exp < 0 ? '-' : '+', abs(exp));
}

static void quoted_string(const char *s, char *buf, int size)
{
    int n = 0;

    if (size < 3)
        return;
    buf[n++] = '"';
    for (; *s && n < size - 6; s++) {
        char c[8];
        char_literal((unsigned char)*s, c, sizeof c, '"');
        for (const char *p = c; *p && n < size - 2; p++)
            buf[n++] = *p;
    }
    buf[n++] = '"';
    buf[n] = '\0';
}

void dbxl_format_value(const dbg_value *v, const char *key, int ptrsize,
                       char *buf, int size)
{
    enum dbxl_style st = own_style(key);
    uint64_t bits = v->bits & mask(v->size);
    int hexw = v->size * 2;

    if (st == STYLE_DEFAULT)
        st = dbxl_default_style(v->kind);
    if (v->kind == DBG_K_POINTER)
        hexw = ptrsize * 2;

    switch (st) {
    case STYLE_ADDRESS:
        if (v->has_addr)
            snprintf(buf, (size_t)size, "@%0*" PRIx64, ptrsize * 2, v->addr);
        else
            snprintf(buf, (size_t)size, "@?");
        return;
    case STYLE_TYPE:
        snprintf(buf, (size_t)size, "%s", v->type_style);
        return;
    case STYLE_SIZE:
        snprintf(buf, (size_t)size, "<%d>", v->size);
        return;
    case STYLE_HEX:
        if (v->kind == DBG_K_POINTER)
            bits = strtoull(v->value, NULL, 16);
        snprintf(buf, (size_t)size, "0x%0*" PRIx64, hexw, bits);
        return;
    default:
        break;
    }

    switch (v->kind) {
    case DBG_K_INT: case DBG_K_UINT: case DBG_K_CHAR: case DBG_K_UCHAR:
    case DBG_K_BOOL: case DBG_K_ENUM: {
        int64_t sv;

        if (v->kind == DBG_K_ENUM && st == STYLE_DEFAULT) {
            snprintf(buf, (size_t)size, "%s", v->value);
            return;
        }
        /* Sign-extend the value's bytes. */
        sv = (int64_t)bits;
        if (v->size > 0 && v->size < 8 && (bits >> (v->size * 8 - 1)) & 1)
            sv = (int64_t)(bits | ~mask(v->size));
        if (st == STYLE_CHARACTER) {
            char c[8];
            char_literal((unsigned)(bits & 0xff), c, sizeof c, '\'');
            snprintf(buf, (size_t)size, "'%s'", c);
        } else if (st == STYLE_UNSIGNED) {
            snprintf(buf, (size_t)size, "%" PRIu64, bits);
        } else {
            snprintf(buf, (size_t)size, "%s%" PRId64, sv >= 0 ? "+" : "", sv);
        }
        return;
    }
    case DBG_K_FLOAT:
        dbxl_format_float(strtod(v->value, NULL), v->size <= 4 ? 6 : 15,
                          st == STYLE_DECIMAL, buf, size);
        return;
    case DBG_K_POINTER:
        if (st == STYLE_STRING && v->str) {
            quoted_string(v->str, buf, size);
            return;
        }
        if (st == STYLE_ARRAY) {
            snprintf(buf, (size_t)size, "[]");
            return;
        }
        snprintf(buf, (size_t)size, "%s",
                 strtoull(v->value, NULL, 16) == 0 ? "<null>" : "ptr");
        return;
    default:
        snprintf(buf, (size_t)size, "%s", v->value ? v->value : "");
        return;
    }
}

/* ---- rendering ---------------------------------------------------------- */

void dbxl_render_init(dbxl_render *r, int ptrsize)
{
    memset(r, 0, sizeof *r);
    r->ptrsize = ptrsize > 0 ? ptrsize : 8;
}

void dbxl_render_free(dbxl_render *r)
{
    for (int i = 0; i < r->nlines; i++)
        free(r->lines[i]);
    free(r->lines);
    free(r->spans);
    for (int i = 0; i < r->nneeds; i++)
        free(r->needs[i]);
    free(r->needs);
    memset(r, 0, sizeof *r);
}

static void new_line(dbxl_render *r, int indent)
{
    if (r->nlines == r->cap) {
        r->cap = r->cap ? r->cap * 2 : 32;
        r->lines = realloc(r->lines, (size_t)r->cap * sizeof *r->lines);
    }
    r->lines[r->nlines] = malloc((size_t)indent + 1);
    memset(r->lines[r->nlines], ' ', (size_t)indent);
    r->lines[r->nlines][indent] = '\0';
    r->nlines++;
}

static int col(const dbxl_render *r)
{
    return (int)strlen(r->lines[r->nlines - 1]);
}

static void put(dbxl_render *r, const char *s)
{
    char **l = &r->lines[r->nlines - 1];
    size_t a = strlen(*l), b = strlen(s);

    *l = realloc(*l, a + b + 1);
    memcpy(*l + a, s, b + 1);
}

static void add_span(dbxl_render *r, int line, int col0, const dbg_value *v,
                     const char *key)
{
    dbxl_span *s;

    if (r->nspans == r->scap) {
        r->scap = r->scap ? r->scap * 2 : 64;
        r->spans = realloc(r->spans, (size_t)r->scap * sizeof *r->spans);
    }
    s = &r->spans[r->nspans++];
    s->line = line;
    s->col0 = col0;
    s->col1 = col(r);
    s->v = v;
    snprintf(s->key, sizeof s->key, "%s", key);
}

static void need(dbxl_render *r, const char *path)
{
    for (int i = 0; i < r->nneeds; i++)
        if (strcmp(r->needs[i], path) == 0)
            return;
    r->needs = realloc(r->needs, (size_t)(r->nneeds + 1) * sizeof *r->needs);
    r->needs[r->nneeds++] = strdup(path);
}

static void render(dbxl_render *r, const dbg_value *v, const char *scope,
                   const char *path, int detail, bool flat);

/* The key of a node: scope + ":" + path. */
static void make_key(char *key, size_t size, const char *scope, const char *path)
{
    snprintf(key, size, "%s:%s", scope, path);
}

/*
 * A child's level is relative to its parent's: each "more" on the child
 * adds to what the parent implies (parent - 1).  So a struct element made
 * inline under a collapsed-children parent goes vertical once the parent
 * itself goes vertical (recon pass 13, m4-14).
 */
static int child_detail(const char *scope, const char *path, int parent)
{
    char key[600];
    int own, d;

    make_key(key, sizeof key, scope, path);
    own = own_detail(key);
    d = parent + own - 2;
    return d > 1 ? d : 1;
}

static void render_aggregate(dbxl_render *r, const dbg_value *v,
                             const char *scope, const char *path, int detail,
                             bool flat)
{
    bool array = v->kind == DBG_K_ARRAY;
    int start = col(r);
    char cpath[520];

    if (detail <= 1) {
        if (array)
            put(r, "[]");
        else {
            put(r, v->tag);
            put(r, "{}");
        }
        return;
    }
    put(r, array ? "[ " : "{ ");
    start = col(r);
    for (int i = 0; i < v->nchildren; i++) {
        const dbg_value *c = &v->children[i];
        int cd;

        if (array)
            snprintf(cpath, sizeof cpath, "%s[%ld]", path, v->lo + i);
        else
            snprintf(cpath, sizeof cpath, "%s.%s", path, c->name);
        cd = child_detail(scope, cpath, detail);

        if (!flat && detail >= 3) {
            char label[64];

            /* Vertical: one per line, aligned after the opening bracket. */
            if (i > 0)
                new_line(r, start);
            if (array)
                snprintf(label, sizeof label, "[%2ld]: ", v->lo + i);
            else
                snprintf(label, sizeof label, "%s: ", c->name);
            put(r, label);
        } else if (i > 0) {
            if (array && i % 10 == 0)
                new_line(r, start);
            else
                put(r, " ");
        }
        render(r, c, scope, cpath, cd, flat);
    }
    put(r, array ? " ]" : " }");
}

static void render(dbxl_render *r, const dbg_value *v, const char *scope,
                   const char *path, int detail, bool flat)
{
    char key[600], text[1100];
    int line = r->nlines - 1, col0 = col(r);
    enum dbxl_style st;

    make_key(key, sizeof key, scope, path);
    st = own_style(key);
    if (st == STYLE_DEFAULT)
        st = dbxl_default_style(v->kind);
    if (dbxl_vstate_get(key, false) && dbxl_vstate_get(key, false)->flat)
        flat = true;

    switch (v->kind) {
    case DBG_K_ARRAY:
    case DBG_K_STRUCT:
    case DBG_K_UNION:
        if (st == STYLE_ADDRESS || st == STYLE_TYPE || st == STYLE_SIZE) {
            dbxl_format_value(v, key, r->ptrsize, text, sizeof text);
            put(r, text);
        } else if (st == STYLE_STRING && v->str) {
            quoted_string(v->str, text, sizeof text);
            put(r, text);
        } else {
            render_aggregate(r, v, scope, path, detail, flat);
        }
        break;
    case DBG_K_POINTER:
        if (st != STYLE_POINTER || strtoull(v->value, NULL, 16) == 0 ||
            (flat ? own_detail(key) : detail) <= 1) {
            dbxl_format_value(v, key, r->ptrsize, text, sizeof text);
            put(r, text);
        } else if (!v->pointee) {
            char p[520];

            snprintf(p, sizeof p, "%s*", path);
            need(r, p);
            put(r, "ptr");
        } else {
            char p[520];
            int d = (flat ? own_detail(key) : detail) - 1;

            snprintf(p, sizeof p, "%s*", path);
            put(r, "->");
            render(r, v->pointee, scope, p, d, false);
        }
        break;
    default:
        dbxl_format_value(v, key, r->ptrsize, text, sizeof text);
        put(r, text);
        break;
    }
    add_span(r, line, col0, v, key);
    /* A span that wrapped covers the rest of its first line. */
    if (r->nlines - 1 != line)
        r->spans[r->nspans - 1].col1 = (int)strlen(r->lines[line]);
}

void dbxl_render_text(dbxl_render *r, const char *text)
{
    new_line(r, 0);
    put(r, text);
}

void dbxl_render_var(dbxl_render *r, const char *scope, const dbg_value *v)
{
    char key[600];
    int d;

    make_key(key, sizeof key, scope, v->name);
    d = own_detail(key);
    new_line(r, 0);
    put(r, v->name);
    put(r, ": ");
    /* The name belongs to the variable too. */
    {
        int line = r->nlines - 1;
        render(r, v, scope, v->name, d, false);
        r->spans[r->nspans - 1].col0 = 0;
        (void)line;
    }
}

const dbxl_span *dbxl_render_hit(const dbxl_render *r, int line, int col)
{
    const dbxl_span *best = NULL;

    for (int i = 0; i < r->nspans; i++) {
        const dbxl_span *s = &r->spans[i];
        if (s->line == line && col >= s->col0 && col < s->col1 &&
            (!best || s->col1 - s->col0 < best->col1 - best->col0))
            best = s;
    }
    return best;
}
