/*
 * Locals, Globals, Monitor and the variable menu (recon passes 3-5, 13).
 *
 * Each pane is rendered from the latest value trees (core/value.c) and
 * keeps its render for hit testing: a left click on an element opens the
 * variable menu for it.  The menu is a pane of its own, 201x460 at frame
 * (336,141) unless the user has moved it, titled with the element's type.
 * It opens with the pointer warped to its first item and closes when an
 * item is chosen (warping the pointer back to the click) or on a click
 * anywhere else.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/layout.h"
#include "core/input.h"
#include "core/options.h"
#include "core/value.h"
#include "data.h"
#include "debugger.h"
#include "machine.h"
#include "ui.h"

enum action {
    A_HEADER, A_MORE, A_LESS, A_FLATTEN, A_STYLE, A_SAVE, A_RECALL,
    A_DEFAULT, A_EDIT, A_BREAKPOINT, A_MONITOR, A_FUNCPARAM, A_STORAGE,
    A_SUBRANGE, A_CAST, A_DOWNCAST, A_SHOWSELF,
};

struct item {
    const char *text;
    enum action act;
    enum dbxl_style style;
};

#define H(t) { t, A_HEADER, STYLE_DEFAULT }
#define S(t, st) { "   - " t, A_STYLE, st }
#define D(t, a) { "   - " t, a, STYLE_DEFAULT }
#define I(t, a) { t, a, STYLE_DEFAULT }
#define END { NULL, A_HEADER, STYLE_DEFAULT }

static const struct item scalar_items[] = {
    H("Style:"), S("character", STYLE_CHARACTER), S("signed", STYLE_SIGNED),
    S("unsigned", STYLE_UNSIGNED), S("hex", STYLE_HEX),
    S("address", STYLE_ADDRESS), S("type", STYLE_TYPE), S("size", STYLE_SIZE),
    D("save", A_SAVE), D("recall", A_RECALL), D("default", A_DEFAULT),
    I("Edit", A_EDIT), I("Breakpoint", A_BREAKPOINT),
    I("Monitor on/off", A_MONITOR), I("Function parameter", A_FUNCPARAM),
    I("Storage view", A_STORAGE), END
};

static const struct item float_items[] = {
    H("Style:"), S("decimal", STYLE_DECIMAL), S("scientific", STYLE_SCIENTIFIC),
    S("hex", STYLE_HEX), S("address", STYLE_ADDRESS), S("type", STYLE_TYPE),
    S("size", STYLE_SIZE), D("save", A_SAVE), D("recall", A_RECALL),
    D("default", A_DEFAULT), I("Edit", A_EDIT), I("Breakpoint", A_BREAKPOINT),
    I("Monitor on/off", A_MONITOR), I("Function parameter", A_FUNCPARAM),
    I("Storage view", A_STORAGE), END
};

static const struct item pointer_items[] = {
    H("Detail:"), D("more", A_MORE), D("less", A_LESS),
    H("Style:"), S("hex", STYLE_HEX), S("string", STYLE_STRING),
    S("array", STYLE_ARRAY), S("pointer", STYLE_POINTER),
    S("address", STYLE_ADDRESS), S("type", STYLE_TYPE), S("size", STYLE_SIZE),
    D("save", A_SAVE), D("recall", A_RECALL), D("default", A_DEFAULT),
    I("Breakpoint", A_BREAKPOINT), I("Select subrange", A_SUBRANGE),
    I("Edit", A_EDIT), I("Cast", A_CAST), I("Downcast", A_DOWNCAST),
    I("Monitor on/off", A_MONITOR), I("Function parameter", A_FUNCPARAM),
    I("Storage view", A_STORAGE), END
};

static const struct item array_items[] = {
    H("Detail:"), D("more", A_MORE), D("less", A_LESS),
    H("Style:"), S("string", STYLE_STRING), S("address", STYLE_ADDRESS),
    S("type", STYLE_TYPE), S("size", STYLE_SIZE), D("save", A_SAVE),
    D("recall", A_RECALL), D("default", A_DEFAULT),
    I("Select subrange", A_SUBRANGE), I("Monitor on/off", A_MONITOR),
    I("Function parameter", A_FUNCPARAM), I("Storage view", A_STORAGE), END
};

static const struct item struct_items[] = {
    H("Detail:"), D("more", A_MORE), D("less", A_LESS),
    D("flatten", A_FLATTEN),
    H("Style:"), S("address", STYLE_ADDRESS), S("type", STYLE_TYPE),
    S("size", STYLE_SIZE), D("save", A_SAVE), D("recall", A_RECALL),
    D("default", A_DEFAULT), I("Show self", A_SHOWSELF),
    I("Monitor on/off", A_MONITOR), I("Function parameter", A_FUNCPARAM),
    I("Storage view", A_STORAGE), END
};

/* A register's menu (title intregister, recon pass 14). */
static const struct item register_items[] = {
    H("Style:"), S("character", STYLE_CHARACTER), S("signed", STYLE_SIGNED),
    S("unsigned", STYLE_UNSIGNED), S("hex", STYLE_HEX), S("type", STYLE_TYPE),
    S("size", STYLE_SIZE), D("save", A_SAVE), D("recall", A_RECALL),
    D("default", A_DEFAULT), I("Edit", A_EDIT), I("Breakpoint", A_BREAKPOINT),
    I("Storage view", A_STORAGE), END
};

enum menu_kind { MK_SCALAR, MK_FLOAT, MK_POINTER, MK_ARRAY, MK_STRUCT,
                 MK_REGISTER, MK_N };

static const struct item *const kind_items[MK_N] = {
    scalar_items, float_items, pointer_items, array_items, struct_items,
    register_items
};

static struct {
    dbg_backend *b;
    dbg_values *locals, *globals;
    char func[256];
    int level;
    bool have_locals;
    dbxl_render r[DBXL_NWINDOWS];
    char *deref_l[256], *deref_g[256];   /* pointer paths followed */
    int nderef_l, nderef_g;
    int ptrsize;

    /* the variable menu */
    xtk_pane *menu;
    bool menu_open;
    enum menu_kind mk;
    dbg_value menu_v;                /* copy of the clicked node's scalars */
    char menu_key[600];
    char menu_expr[1100];
    char menu_text[256];             /* the value as displayed */
    char menu_label[1100];            /* the object, for trigger entries */
    enum dbxl_style menu_style;      /* its style when the menu opened */
    xtk_rect menu_geom;              /* the menu's place, for the dialogs */
    uint64_t menu_addr;              /* for Storage view */
    bool menu_has_addr;
    uint64_t param;                  /* Function parameter, for func() */
    bool have_param;
    bool menu_signed;                /* the style compares signed */
    int ax, ay;                      /* the click that opened it */
    char last_chosen[32];            /* the item chosen last, any menu */
    char kind_last[MK_N][32];        /* ... per kind of menu */
    enum dbxl_style saved_style;     /* "save": one for all objects */
    char **types;                    /* for Cast */
    int ntypes;
    bool cast_menu;                  /* the menu lists types (Cast) */
    char subrange_text[64];          /* the prompt's initial text */
    long arr_lo, arr_hi;             /* the clicked array's bounds */
    bool have_saved;
} dd;

/* ---- rendering ---------------------------------------------------------- */

static void locals_scope(char *buf, size_t size)
{
    snprintf(buf, size, "L:%s", dd.func);
}

static void globals_scope(const dbg_value_group *g, char *buf, size_t size)
{
    snprintf(buf, size, "G:%s", g->file ? g->file : "");
}

static bool is_monitored(const char *scope, const dbg_value *v)
{
    char key[600];
    dbxl_vstate *s;

    snprintf(key, sizeof key, "%s:%s", scope, v->name);
    s = dbxl_vstate_get(key, false);
    return s && s->monitored;
}

static void show(int window, dbxl_render *r)
{
    xtk_pane *p = dbxl_ui_pane(window);
    int top = xtk_pane_top(p);

    xtk_pane_set_lines(p, (const char *const *)r->lines, r->nlines);
    /* Keep the scroll position across refreshes. */
    xtk_pane_set_top(p, top < r->nlines ? top : 0);
}

static void request(enum dbg_scope scope);

/* Remember newly needed pointer paths; returns true if any were new. */
static bool add_needs(const dbxl_render *r, char **set, int *n)
{
    bool added = false;

    for (int i = 0; i < r->nneeds; i++) {
        bool have = false;
        for (int k = 0; k < *n; k++)
            if (strcmp(set[k], r->needs[i]) == 0)
                have = true;
        if (!have && *n < 256) {
            set[(*n)++] = strdup(r->needs[i]);
            added = true;
        }
    }
    return added;
}

static void render_all(void)
{
    char scope[300];
    dbxl_render *rl = &dd.r[DBXL_W_LOCALS], *rg = &dd.r[DBXL_W_GLOBALS],
                *rm = &dd.r[DBXL_W_MONITOR];
    bool refetch_l = false, refetch_g = false;

    dbxl_render_free(rl);
    dbxl_render_free(rg);
    dbxl_render_free(rm);
    dbxl_render_init(rl, dd.ptrsize);
    dbxl_render_init(rg, dd.ptrsize);
    dbxl_render_init(rm, dd.ptrsize);

    locals_scope(scope, sizeof scope);
    if (dd.locals && dd.have_locals)
        for (int i = 0; i < dd.locals->groups[0].nvars; i++)
            dbxl_render_var(rl, scope, &dd.locals->groups[0].vars[i]);

    /* Globals: a blank row, the file header, a blank row, the values. */
    if (dd.globals)
        for (int g = 0; g < dd.globals->ngroups; g++) {
            const dbg_value_group *grp = &dd.globals->groups[g];
            char head[600];

            globals_scope(grp, scope, sizeof scope);
            snprintf(head, sizeof head, "----- File %s -----", grp->file);
            dbxl_render_text(rg, "");
            dbxl_render_text(rg, head);
            dbxl_render_text(rg, "");
            for (int i = 0; i < grp->nvars; i++)
                dbxl_render_var(rg, scope, &grp->vars[i]);
        }

    /* Monitor: the monitored globals, then (with a frame) the locals. */
    dbxl_render_text(rm, "----- Globals -----");
    if (dd.globals)
        for (int g = 0; g < dd.globals->ngroups; g++) {
            const dbg_value_group *grp = &dd.globals->groups[g];
            globals_scope(grp, scope, sizeof scope);
            for (int i = 0; i < grp->nvars; i++)
                if (is_monitored(scope, &grp->vars[i]))
                    dbxl_render_var(rm, scope, &grp->vars[i]);
        }
    if (dd.locals && dd.have_locals) {
        dbxl_render_text(rm, "");
        dbxl_render_text(rm, "----- Locals -----");
        locals_scope(scope, sizeof scope);
        for (int i = 0; i < dd.locals->groups[0].nvars; i++)
            if (is_monitored(scope, &dd.locals->groups[0].vars[i]))
                dbxl_render_var(rm, scope, &dd.locals->groups[0].vars[i]);
    }

    show(DBXL_W_LOCALS, rl);
    show(DBXL_W_GLOBALS, rg);
    show(DBXL_W_MONITOR, rm);

    /* Pointers shown expanded whose targets we don't have yet. */
    refetch_l = add_needs(rl, dd.deref_l, &dd.nderef_l);
    refetch_g = add_needs(rg, dd.deref_g, &dd.nderef_g);
    if (add_needs(rm, dd.deref_l, &dd.nderef_l))
        refetch_l = true;
    if (add_needs(rm, dd.deref_g, &dd.nderef_g))
        refetch_g = true;
    if (refetch_l && dd.have_locals)
        request(DBG_SCOPE_LOCALS);
    if (refetch_g)
        request(DBG_SCOPE_GLOBALS);
}

/* ---- fetching ------------------------------------------------------------ */

/* Subranges and casts travel with the pointer paths ("range:", "cast:"). */
struct special {
    char **list;
    int n;
};

static void collect_special(const char *key, dbxl_vstate *st, void *arg)
{
    struct special *sp = arg;
    const char *path = strchr(key, ':');
    char buf[800];

    path = path ? strchr(path + 1, ':') : NULL;
    if (!path || sp->n >= 250)
        return;
    path++;
    if (st->has_range) {
        snprintf(buf, sizeof buf, "range:%s:%ld:%ld", path, st->lo, st->hi);
        sp->list[sp->n++] = strdup(buf);
    }
    if (st->cast[0] && sp->n < 250) {
        snprintf(buf, sizeof buf, "cast:%s:%s", path, st->cast);
        sp->list[sp->n++] = strdup(buf);
    }
}

static void request(enum dbg_scope scope)
{
    char prefix[300];
    char **all = scope == DBG_SCOPE_LOCALS ? dd.deref_l : dd.deref_g;
    int nall = scope == DBG_SCOPE_LOCALS ? dd.nderef_l : dd.nderef_g;
    struct special sp;

    if (!dd.b)
        return;
    sp.list = calloc((size_t)nall + 256, sizeof *sp.list);
    for (int i = 0; i < nall; i++)
        sp.list[i] = strdup(all[i]);
    sp.n = nall;
    if (scope == DBG_SCOPE_LOCALS)
        snprintf(prefix, sizeof prefix, "L:%s:", dd.func);
    else
        snprintf(prefix, sizeof prefix, "G:");
    dbxl_vstate_foreach(prefix, collect_special, &sp);
    dd.b->ops->values(dd.b, scope, scope == DBG_SCOPE_LOCALS ? dd.level : 0,
                      (const char *const *)sp.list, sp.n,
                      scope == DBG_SCOPE_LOCALS && dbxl_opt.local_all
                          ? DBG_VALUES_ALL_BLOCKS : 0, NULL);
    for (int i = 0; i < sp.n; i++)
        free(sp.list[i]);
    free(sp.list);
}

void dbxl_data_types(const char *const *names, int n)
{
    for (int i = 0; i < dd.ntypes; i++)
        free(dd.types[i]);
    free(dd.types);
    dd.types = calloc((size_t)n + 1, sizeof *dd.types);
    for (int i = 0; i < n; i++)
        dd.types[i] = strdup(names[i]);
    dd.ntypes = n;
}

void dbxl_data_set_backend(dbg_backend *b)
{
    dd.b = b;
}

void dbxl_data_refresh(const char *func, int level)
{
    if (strcmp(dd.func, func) != 0) {
        /* Another function: its own pointer paths. */
        for (int i = 0; i < dd.nderef_l; i++)
            free(dd.deref_l[i]);
        dd.nderef_l = 0;
    }
    snprintf(dd.func, sizeof dd.func, "%s", func);
    dd.level = level;
    dd.have_locals = func[0] != '\0';
    if (dd.have_locals)
        request(DBG_SCOPE_LOCALS);
    request(DBG_SCOPE_GLOBALS);
    if (!dd.have_locals)
        render_all();
}

void dbxl_data_clear_locals(void)
{
    dd.have_locals = false;
    render_all();
}

void dbxl_data_clear_all(void)
{
    dd.have_locals = false;
    dbg_values_free(dd.locals);
    dd.locals = NULL;
    dbg_values_free(dd.globals);
    dd.globals = NULL;
    render_all();
}

int dbxl_data_ptrsize(void)
{
    return dd.ptrsize;
}

bool dbxl_data_function_parameter(uint64_t *addr)
{
    *addr = dd.param;
    return dd.have_param;
}

void dbxl_data_values(dbg_values *v)
{
    if (v->scope == DBG_SCOPE_LOCALS) {
        if (v->level != dd.level) {       /* a stale answer */
            dbg_values_free(v);
            return;
        }
        dbg_values_free(dd.locals);
        dd.locals = v;
    } else {
        dbg_values_free(dd.globals);
        dd.globals = v;
    }
    dd.ptrsize = v->ptrsize;
    render_all();
}

void dbxl_data_assigned(void)
{
    if (dd.have_locals)
        request(DBG_SCOPE_LOCALS);
    request(DBG_SCOPE_GLOBALS);
}

/* ---- the variable menu ------------------------------------------------- */

static enum menu_kind kind_of(const dbg_value *v)
{
    switch (v->kind) {
    case DBG_K_FLOAT: return MK_FLOAT;
    case DBG_K_POINTER: return MK_POINTER;
    case DBG_K_ARRAY: return MK_ARRAY;
    case DBG_K_STRUCT: case DBG_K_UNION: return MK_STRUCT;
    case DBG_K_REGISTER: return MK_REGISTER;
    default: return MK_SCALAR;
    }
}

static const char *style_name(enum dbxl_style st)
{
    switch (st) {
    case STYLE_CHARACTER: return "character";
    case STYLE_SIGNED: return "signed";
    case STYLE_UNSIGNED: return "unsigned";
    case STYLE_SCIENTIFIC: return "scientific";
    default: return "";
    }
}

/* The item text without its "   - " prefix. */
static const char *item_name(const struct item *it)
{
    return strncmp(it->text, "   - ", 5) == 0 ? it->text + 5 : it->text;
}

static int find_item(const struct item *items, const char *name)
{
    for (int i = 0; items[i].text; i++)
        if (items[i].act != A_HEADER && strcmp(item_name(&items[i]), name) == 0)
            return i;
    return -1;
}

/*
 * The highlighted item: the last one chosen, if this menu has it, else
 * the last chosen in this kind of menu, else the kind's first choice
 * (more; flatten for structs; a scalar's current style).
 */
static int default_item(const struct item *items, const dbg_value *v)
{
    int i;

    if (dd.last_chosen[0] && (i = find_item(items, dd.last_chosen)) >= 0)
        return i;
    if (dd.kind_last[dd.mk][0] && (i = find_item(items, dd.kind_last[dd.mk])) >= 0)
        return i;
    switch (dd.mk) {
    case MK_STRUCT: return find_item(items, "flatten");
    case MK_REGISTER: return find_item(items, "hex");
    case MK_POINTER: case MK_ARRAY: return find_item(items, "more");
    default:
        i = find_item(items, style_name(dbxl_default_style(v)));
        return i >= 0 ? i : 1;
    }
}

static void close_menu(bool warp_back)
{
    if (!dd.menu_open)
        return;
    xtk_pane_unmap(dd.menu);
    dd.menu_open = false;
    if (warp_back)
        xtk_warp_frame(dd.ax, dd.ay);
}

static void open_menu(const dbxl_span *s, int ax, int ay)
{
    const struct item *items;
    const char *lines[32];
    unsigned char roles[32];
    int n = 0, sel;
    xtk_rect g;

    dd.mk = kind_of(s->v);
    items = kind_items[dd.mk];
    for (; items[n].text && n < 32; n++) {
        lines[n] = items[n].text;
        roles[n] = items[n].act == A_HEADER ? XTK_ROLE_INFO : XTK_ROLE_NORMAL;
    }
    snprintf(dd.menu_key, sizeof dd.menu_key, "%s", s->key);
    snprintf(dd.menu_expr, sizeof dd.menu_expr, "%s", s->v->expr ? s->v->expr : "");
    dbxl_format_value(s->v, s->key, dd.ptrsize, dd.menu_text, sizeof dd.menu_text);
    dd.menu_v.kind = s->v->kind;
    dd.menu_v.size = s->v->size;
    dd.cast_menu = false;
    dd.arr_lo = s->v->lo;
    dd.arr_hi = s->v->lo + (s->v->nchildren > 0 ? s->v->nchildren - 1 : 0);
    {
        dbxl_vstate *vs = dbxl_vstate_get(s->key, false);
        dd.menu_style = vs ? vs->style : STYLE_DEFAULT;
    }
    {
        dbxl_vstate *st = dbxl_vstate_get(s->key, false);
        enum dbxl_style style = st && st->style ? st->style
                                                : dbxl_default_style(s->v);
        const char *path = strchr(s->key, ':');

        path = path ? strchr(path + 1, ':') : NULL;
        path = path ? path + 1 : s->key;
        /* A variable by its name, an element by its expression. */
        snprintf(dd.menu_label, sizeof dd.menu_label, "%s",
                 strpbrk(path, ".[*") ? dd.menu_expr : s->v->name);
        dd.menu_signed = style == STYLE_CHARACTER || style == STYLE_SIGNED ||
                         style == STYLE_DECIMAL || style == STYLE_SCIENTIFIC;
        /* A register's Storage view goes to the address it holds. */
        dd.menu_has_addr = s->v->kind == DBG_K_REGISTER || s->v->has_addr;
        dd.menu_addr = s->v->kind == DBG_K_REGISTER ? s->v->bits : s->v->addr;
    }
    dd.ax = ax;
    dd.ay = ay;
    sel = default_item(items, s->v);

    xtk_pane_set_title(dd.menu, s->v->type_name);
    xtk_pane_set_lines(dd.menu, lines, n);
    xtk_pane_set_line_roles(dd.menu, roles, n);
    xtk_pane_set_selected(dd.menu, sel);
    xtk_pane_map(dd.menu);
    dd.menu_open = true;
    /* The pointer goes to (4, 32) inside the menu: its first item. */
    g = xtk_pane_geometry(dd.menu);
    xtk_warp_frame(g.x + 2 + 4, g.y + 2 + 32);
}

/* The root variable's key of an element's key ("L:main:sp*.name[2]"). */
static void root_key(const char *key, char *buf, size_t size)
{
    const char *path = strchr(key, ':');
    size_t n;

    path = path ? strchr(path + 1, ':') : NULL;
    if (!path) {
        snprintf(buf, size, "%s", key);
        return;
    }
    n = (size_t)(path + 1 - key) + strcspn(path + 1, ".[*");
    snprintf(buf, size, "%.*s", (int)n, key);
}

static void edit_done(int button, const char *text, void *arg);
static void trigger_done(int button, const char *text, void *arg);
static void subrange_done(int button, const char *text, void *arg);
static void open_cast_menu(void);

/*
 * xldb reads typed values in the object's display style.  A bad one shows
 * the style's hint and asks again with the text kept (recon pass 15).
 */
static void ask_again(const char *prompt, const char *text, const char *msg,
                      xtk_dialog_fn fn)
{
    dbxl_ui_message(msg);
    xtk_dialog_prompt_at(prompt, text, "proceed", "cancel", dd.menu_geom.x,
                         dd.menu_geom.y, dd.ax, dd.ay, fn, NULL);
}

static void edit_done(int button, const char *text, void *arg)
{
    dbxl_input in;
    char msg[256], expr[1300];

    (void)arg;
    if (button != 0 || !dd.b || !dd.menu_expr[0])
        return;
    if (dbxl_parse_input(&dd.menu_v, dd.menu_style, text, &in, msg,
                         sizeof msg) < 0) {
        ask_again("Enter new value:", text, msg, edit_done);
        return;
    }
    if (in.bits)                     /* a float's bits, typed in hex */
        snprintf(expr, sizeof expr, "*(%s *)&(%s)",
                 dd.menu_v.size > 4 ? "unsigned long long" : "unsigned int",
                 dd.menu_expr);
    else
        snprintf(expr, sizeof expr, "%s", dd.menu_expr);
    dd.b->ops->assign(dd.b, expr, in.literal, NULL);
}

/* One trigger value in the object's style, into buf ("" and -1 if bad). */
static int trigger_value(const char *text, char *buf, size_t size, char *msg,
                         size_t msgsize)
{
    dbxl_input in;

    if (dbxl_parse_input(&dd.menu_v, dd.menu_style, text, &in, msg, msgsize) < 0)
        return -1;
    snprintf(buf, size, "%s", in.literal);
    return 0;
}

static void trigger_done(int button, const char *text, void *arg)
{
    char t[256], lo[300], hi[300], cond[700], msg[256];
    const char *p = text, *dots;
    bool neg = false;

    (void)arg;
    if (button != 0 || !dd.menu_expr[0])
        return;
    while (*p == ' ')
        p++;
    if (*p == '!') {
        neg = true;
        p++;
    }
    snprintf(t, sizeof t, "%s", p);
    dots = strstr(t, "..");
    if (dots) {
        char left[256];

        snprintf(left, sizeof left, "%.*s", (int)(dots - t), t);
        if (trigger_value(left, lo, sizeof lo, msg, sizeof msg) < 0 ||
            trigger_value(dots + 2, hi, sizeof hi, msg, sizeof msg) < 0) {
            ask_again("Enter breakpoint trigger:", text, msg, trigger_done);
            return;
        }
        snprintf(cond, sizeof cond, "%s%s..%s", neg ? "!" : "", lo, hi);
    } else {
        if (trigger_value(t, lo, sizeof lo, msg, sizeof msg) < 0) {
            ask_again("Enter breakpoint trigger:", text, msg, trigger_done);
            return;
        }
        snprintf(cond, sizeof cond, "%s%s", neg ? "!" : "", lo);
    }
    dbxl_debug_break_trigger(dd.menu_label, dd.menu_expr, text, cond,
                             dd.menu_signed);
}

/* The first more (or flatten) on an object goes to detail 2 whatever the
 * Detail per click; later ones add the step (recon pass 16). */
static int more_detail(dbxl_vstate *st, int step, int max)
{
    int d = st->stepped ? st->detail + step : 2;

    st->stepped = true;
    return d < max ? d : max;
}

static void choose(int row)
{
    const struct item *items = kind_items[dd.mk];
    const struct item *it;
    dbxl_vstate *st;
    char key[600];
    xtk_rect g;
    int max, step;

    for (int i = 0; i <= row; i++)
        if (!items[i].text)
            return;
    it = &items[row];
    if (it->act == A_HEADER)
        return;                         /* headers don't respond */
    snprintf(dd.last_chosen, sizeof dd.last_chosen, "%s", item_name(it));
    snprintf(dd.kind_last[dd.mk], sizeof dd.kind_last[dd.mk], "%s", item_name(it));
    st = dbxl_vstate_get(dd.menu_key, true);
    /* Levels beyond a value's own depth carry on into its components
     * (relative levels, recon pass 13), so the cap is generous. */
    max = 20;
    step = dbxl_opt.detail_per_click > 0 ? dbxl_opt.detail_per_click : 1;
    g = xtk_pane_geometry(dd.menu);
    dd.menu_geom = g;
    /* The dialogs warp the pointer themselves (and back when they close). */
    close_menu(it->act != A_EDIT && it->act != A_BREAKPOINT &&
               it->act != A_SUBRANGE && it->act != A_CAST);

    switch (it->act) {
    case A_MORE:
        if (dd.mk == MK_POINTER)
            st->style = STYLE_DEFAULT;  /* back to the pointer view */
        st->flat = false;
        st->detail = more_detail(st, step, max);
        break;
    case A_LESS:
        if (dd.mk == MK_POINTER)
            st->style = STYLE_DEFAULT;
        st->flat = false;
        st->detail = st->detail - step < 1 ? 1 : st->detail - step;
        st->stepped = true;
        break;
    case A_FLATTEN:
        /* Like more, but horizontal (xldb's help). */
        st->flat = true;
        st->detail = more_detail(st, step, max);
        break;
    case A_STYLE:
        st->style = it->style == dbxl_default_style(&dd.menu_v)
                        ? STYLE_DEFAULT : it->style;
        break;
    case A_SAVE:
        /* One saved style for all objects (recon pass 15). */
        dd.saved_style = st->style;
        dd.have_saved = true;
        dbxl_ui_message("Use \"recall\" to apply the saved style to an object.");
        return;
    case A_RECALL:
        if (!dd.have_saved) {
            dbxl_ui_message("Use \"save\" to save a style before using "
                            "\"recall\".");
            return;
        }
        st->style = dd.saved_style;
        break;
    case A_SUBRANGE: {
        dbxl_vstate *vs = dbxl_vstate_get(dd.menu_key, false);

        /* A pointer starts at [0..0]; an array at its current range. */
        if (vs && vs->has_range)
            snprintf(dd.subrange_text, sizeof dd.subrange_text, "[%ld..%ld]",
                     vs->lo, vs->hi);
        else if (dd.menu_v.kind == DBG_K_ARRAY)
            snprintf(dd.subrange_text, sizeof dd.subrange_text, "[%ld..%ld]",
                     dd.arr_lo, dd.arr_hi);
        else
            snprintf(dd.subrange_text, sizeof dd.subrange_text, "[0..0]");
        xtk_dialog_prompt_at("Specify array subrange(s)", dd.subrange_text,
                             "proceed", "cancel", g.x, g.y, dd.ax, dd.ay,
                             subrange_done, NULL);
        xtk_dialog_set_cursor(0);
        return;
    }
    case A_CAST:
        open_cast_menu();
        return;
    case A_FUNCPARAM:
        /* The address goes to the next commandList func() call. */
        if (dd.menu_has_addr) {
            dd.param = dd.menu_addr;
            dd.have_param = true;
        }
        dbxl_ui_message("Address of selected object will be argument for "
                        "next user command invocation");
        return;
    case A_DEFAULT:
        st->style = STYLE_DEFAULT;
        break;
    case A_EDIT:
        if (dbxl_debug_core()) {
            /* At once, with no dialog (recon pass 17). */
            dbxl_ui_message("Modification not allowed with core files");
            return;
        }
        xtk_dialog_prompt_at("Enter new value:", dd.menu_text, "proceed",
                             "cancel", g.x, g.y, dd.ax, dd.ay, edit_done, NULL);
        return;
    case A_MONITOR:
        root_key(dd.menu_key, key, sizeof key);
        st = dbxl_vstate_get(key, true);
        st->monitored = !st->monitored;
        break;
    case A_STORAGE:
        if (dd.menu_has_addr)
            dbxl_machine_storage_view(dd.menu_addr);
        return;
    case A_BREAKPOINT:
        xtk_dialog_prompt_at("Enter breakpoint trigger:", dd.menu_text,
                             "proceed", "cancel", g.x, g.y, dd.ax, dd.ay,
                             trigger_done, NULL);
        return;
    default:
        /* Breakpoint triggers, subranges, casts, Show self, Function
         * parameter: later milestones. */
        return;
    }
    render_all();
}

static void menu_event(xtk_pane *p, const xtk_pane_event *e, void *arg)
{
    (void)p;
    (void)arg;
    if (e->type != XTK_PANE_CLICK || e->button != Button1)
        return;
    if (dd.cast_menu) {
        /* Cast: the chosen type becomes what the pointer points to. */
        if (e->line >= 0 && e->line < dd.ntypes) {
            dbxl_vstate *st = dbxl_vstate_get(dd.menu_key, true);
            snprintf(st->cast, sizeof st->cast, "%s", dd.types[e->line]);
            close_menu(true);
            dd.cast_menu = false;
            request(dd.menu_key[0] == 'G' ? DBG_SCOPE_GLOBALS : DBG_SCOPE_LOCALS);
        }
        return;
    }
    choose(e->line);
}

/* Cast: the Formats window lists the program's types (recon pass 15). */
static void open_cast_menu(void)
{
    xtk_rect g;
    unsigned char *roles = calloc((size_t)dd.ntypes + 1, 1);

    dd.cast_menu = true;
    xtk_pane_set_title(dd.menu, "Select new base type");
    xtk_pane_set_lines(dd.menu, (const char *const *)dd.types, dd.ntypes);
    xtk_pane_set_line_roles(dd.menu, roles, dd.ntypes);
    xtk_pane_set_selected(dd.menu, dd.ntypes > 0 ? 0 : -1);
    xtk_pane_map(dd.menu);
    dd.menu_open = true;
    g = xtk_pane_geometry(dd.menu);
    xtk_warp_frame(g.x + 2 + 4, g.y + 2 + 13 + 6);
    free(roles);
}

/*
 * Select subrange: `[lo..hi]`, `[n]`, `[*]` or `[lo..*]` (an array); a
 * pointer needs an upper bound.  xldb's hints come from its catalogue.
 */
static void subrange_done(int button, const char *text, void *arg)
{
    char t[128], *p, *dots;
    long lo, hi;
    bool pointer = dd.menu_v.kind == DBG_K_POINTER;
    dbxl_vstate *st;

    (void)arg;
    if (button != 0)
        return;
    snprintf(t, sizeof t, "%s", text);
    p = t;
    while (*p == ' ')
        p++;
    if (*p != '[' || !strchr(p, ']')) {
        ask_again("Specify array subrange(s)", text,
                  "Specify a range enclosed in brackets [].", subrange_done);
        return;
    }
    p++;
    *strchr(p, ']') = '\0';
    dots = strstr(p, "..");
    if (dots) {
        *dots = '\0';
        lo = strcmp(p, "*") == 0 ? dd.arr_lo : strtol(p, NULL, 10);
        if (strcmp(dots + 2, "*") == 0) {
            if (pointer) {
                ask_again("Specify array subrange(s)", text,
                          "An upper bound must be specified because the "
                          "range is unbounded", subrange_done);
                return;
            }
            hi = dd.arr_hi;
        } else {
            hi = strtol(dots + 2, NULL, 10);
        }
    } else if (strcmp(p, "*") == 0 && !pointer) {
        lo = dd.arr_lo;
        hi = dd.arr_hi;
    } else if (*p >= '0' && *p <= '9') {
        lo = hi = strtol(p, NULL, 10);
    } else {
        ask_again("Specify array subrange(s)", text,
                  "Enter a range or index: specify \"i\", \"i..j\", \"*..j\", "
                  "\"i..*\", or \"*\".", subrange_done);
        return;
    }
    if (hi < lo || hi - lo > 999) {
        char msg[128];
        snprintf(msg, sizeof msg, "The range %ld .. %ld is too large.", lo, hi);
        ask_again("Specify array subrange(s)", text, msg, subrange_done);
        return;
    }
    st = dbxl_vstate_get(dd.menu_key, true);
    st->has_range = true;
    st->lo = lo;
    st->hi = hi;
    request(dd.menu_key[0] == 'G' ? DBG_SCOPE_GLOBALS : DBG_SCOPE_LOCALS);
}

void dbxl_data_open_menu(const dbxl_span *s, int fx, int fy)
{
    open_menu(s, fx, fy);
}

void dbxl_data_click(int window, const xtk_pane_event *e)
{
    const dbxl_span *s = dbxl_render_hit(&dd.r[window], e->line, e->col);

    if (!s) {
        dbxl_ui_message("Click on a data object.");
        return;
    }
    open_menu(s, e->fx, e->fy);
}

bool dbxl_data_handle_event(const XEvent *ev)
{
    if (xtk_pane_handle_event(dd.menu, ev))
        return true;
    /* A press anywhere else closes the menu; the press still counts. */
    if (dd.menu_open && ev->type == ButtonPress &&
        ev->xbutton.window != xtk_pane_window(dd.menu))
        close_menu(false);
    return false;
}

void dbxl_data_init(xtk_rect formats)
{
    dd.ptrsize = 8;
    dd.menu = xtk_pane_create("", formats,
                              XTK_SB_NONE);
    xtk_pane_set_handler(dd.menu, menu_event, NULL);
    for (int w = 0; w < DBXL_NWINDOWS; w++)
        dbxl_render_init(&dd.r[w], 8);
}

xtk_rect dbxl_data_formats_geometry(void)
{
    return xtk_pane_geometry(dd.menu);
}
