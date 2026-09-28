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
#include "core/value.h"
#include "data.h"
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

enum menu_kind { MK_SCALAR, MK_FLOAT, MK_POINTER, MK_ARRAY, MK_STRUCT, MK_N };

static const struct item *const kind_items[MK_N] = {
    scalar_items, float_items, pointer_items, array_items, struct_items
};

#define MENU_X 336
#define MENU_Y 141
#define MENU_W 201
#define MENU_H 460

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
    int ax, ay;                      /* the click that opened it */
    char last_chosen[32];            /* the item chosen last, any menu */
    char kind_last[MK_N][32];        /* ... per kind of menu */
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

static void request(enum dbg_scope scope)
{
    if (!dd.b)
        return;
    if (scope == DBG_SCOPE_LOCALS)
        dd.b->ops->values(dd.b, scope, dd.level,
                          (const char *const *)dd.deref_l, dd.nderef_l, NULL);
    else
        dd.b->ops->values(dd.b, scope, 0,
                          (const char *const *)dd.deref_g, dd.nderef_g, NULL);
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
    case MK_POINTER: case MK_ARRAY: return find_item(items, "more");
    default:
        i = find_item(items, style_name(dbxl_default_style(v->kind)));
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

static void edit_done(int button, const char *text, void *arg)
{
    (void)arg;
    if (button != 0 || !dd.b || !dd.menu_expr[0])
        return;
    dd.b->ops->assign(dd.b, dd.menu_expr, text, NULL);
}

static void choose(int row)
{
    const struct item *items = kind_items[dd.mk];
    const struct item *it;
    dbxl_vstate *st;
    char key[600];
    xtk_rect g;
    int max;

    for (int i = 0; i <= row; i++)
        if (!items[i].text)
            return;
    it = &items[row];
    if (it->act == A_HEADER)
        return;                         /* headers don't respond */
    snprintf(dd.last_chosen, sizeof dd.last_chosen, "%s", item_name(it));
    snprintf(dd.kind_last[dd.mk], sizeof dd.kind_last[dd.mk], "%s", item_name(it));
    st = dbxl_vstate_get(dd.menu_key, true);
    max = dd.mk == MK_POINTER ? 4 : 3;
    g = xtk_pane_geometry(dd.menu);
    /* Edit's dialog warps the pointer itself (and back when it closes). */
    close_menu(it->act != A_EDIT);

    switch (it->act) {
    case A_MORE:
        if (dd.mk == MK_POINTER)
            st->style = STYLE_DEFAULT;  /* back to the pointer view */
        st->flat = false;
        st->detail = st->detail < max ? st->detail + 1 : max;
        break;
    case A_LESS:
        if (dd.mk == MK_POINTER)
            st->style = STYLE_DEFAULT;
        st->flat = false;
        st->detail = st->detail <= 1 ? 1 : st->detail - 1;
        break;
    case A_FLATTEN:
        /* Like more, but horizontal (xldb's help). */
        st->flat = true;
        st->detail = st->detail < max ? st->detail + 1 : max;
        break;
    case A_STYLE:
        st->style = it->style == dbxl_default_style(dd.menu_v.kind)
                        ? STYLE_DEFAULT : it->style;
        break;
    case A_SAVE:
        st->saved = st->style;
        break;
    case A_RECALL:
        st->style = st->saved;
        break;
    case A_DEFAULT:
        st->style = STYLE_DEFAULT;
        break;
    case A_EDIT:
        xtk_dialog_prompt_at("Enter new value:", dd.menu_text, "proceed",
                             "cancel", g.x, g.y, dd.ax, dd.ay, edit_done, NULL);
        return;
    case A_MONITOR:
        root_key(dd.menu_key, key, sizeof key);
        st = dbxl_vstate_get(key, true);
        st->monitored = !st->monitored;
        break;
    case A_STORAGE:
        if (!xtk_pane_mapped(dbxl_ui_pane(DBXL_W_STORAGE)))
            dbxl_ui_message("Storage view ignored. Storage pane is hidden");
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
    if (e->type == XTK_PANE_CLICK && e->button == Button1)
        choose(e->line);
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

void dbxl_data_init(void)
{
    dd.ptrsize = 8;
    dd.menu = xtk_pane_create("", (xtk_rect){ MENU_X, MENU_Y, MENU_W, MENU_H },
                              XTK_SB_NONE);
    xtk_pane_set_handler(dd.menu, menu_event, NULL);
    for (int w = 0; w < DBXL_NWINDOWS; w++)
        dbxl_render_init(&dd.r[w], 8);
}
