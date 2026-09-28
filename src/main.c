/*
 * dbxl - a Linux clone of IBM's AIX xldb debugger.
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <X11/Xutil.h>

#include "backend/backend.h"
#include "core/value.h"

#include "core/layout.h"
#include "core/options.h"
#include "core/resources.h"
#include "debugger.h"
#include "help.h"
#include "opts.h"
#include "ui.h"
#include "xtk/xtk.h"

static Atom wm_protocols, wm_delete;

static bool close_event(const XEvent *ev, void *arg)
{
    (void)arg;
    if (ev->type == ClientMessage &&
        ev->xclient.message_type == wm_protocols &&
        (Atom)ev->xclient.data.l[0] == wm_delete) {
        xtk_loop_quit(0);
        return true;
    }
    return false;
}

/*
 * The frame geometry in logical pixels.  Negative offsets count from the
 * right/bottom of the screen measured in logical pixels, so -geometry means
 * the same layout at every scale.  With no screen size yet (0) only the
 * size is resolved.
 */
static xtk_rect frame_geometry(const char *spec, int screen_w, int screen_h)
{
    xtk_rect g = dbxl_frame_geometry;
    int gx, gy;
    unsigned gw, gh;
    int mask, outer = 4;          /* 2px border on each side */

    if (!spec)
        return g;
    mask = XParseGeometry(spec, &gx, &gy, &gw, &gh);
    if (mask & WidthValue)
        g.w = (int)gw;
    if (mask & HeightValue)
        g.h = (int)gh;
    if (mask & XValue)
        g.x = (mask & XNegative) ? screen_w + gx - g.w - outer : gx;
    if (mask & YValue)
        g.y = (mask & YNegative) ? screen_h + gy - g.h - outer : gy;
    return g;
}

/* Colour resources and the roles they set (DESIGN.md 5.1). */
static const struct {
    const char *resource;
    enum xtk_color role;
} color_resources[] = {
    { "background", XTK_BG },
    { "foreground", XTK_FG },
    { "titleBackground", XTK_TITLE_BG },
    { "titleForeground", XTK_TITLE_FG },
    { "borderActive", XTK_BORDER_ACTIVE },
    { "borderIdle", XTK_BORDER_IDLE },
    { "dialogBackground", XTK_DIALOG_BG },
    { "dialogForeground", XTK_DIALOG_FG },
    { "menuInfoForeground", XTK_MENUINFO_FG },
    { "markBackground", XTK_MARK_BG },
    { "markForeground", XTK_MARK_FG },
    { "cursorForeground", XTK_CURSOR },
    { "mouseBody", XTK_POINTER_BODY },
    { "mouseOutline", XTK_POINTER_OUTLINE },
    { "scrollButtonIdle", XTK_THUMB },
};

/* The first word of a resource, compared without case. */
static bool res_is(const char *resource, const char *word)
{
    const char *s = dbxl_res_str(resource);
    size_t n = strlen(word);

    if (!s)
        return false;
    s += strspn(s, " \t");
    return strncasecmp(s, word, n) == 0 &&
           (s[n] == '\0' || s[n] == ' ' || s[n] == '\t');
}

/* Options and display settings from resources (the help's list). */
static void apply_resources(void)
{
    const char *s;

    if ((s = dbxl_res_str("sourceSearchPath")))
        snprintf(dbxl_opt.source_path, sizeof dbxl_opt.source_path, "%s ", s);
    dbxl_opt.local_all = res_is("localVariables", "all");
    dbxl_opt.detail_per_click = dbxl_res_int("detailPerClick", 1);
    if (dbxl_opt.detail_per_click < 1)
        dbxl_opt.detail_per_click = 1;
    dbxl_opt.case_sensitive = dbxl_res_bool("caseSensitive", false);
    dbxl_opt.multiprocess = dbxl_res_bool("multiprocessDebug", true);
    dbxl_opt.fork_path = res_is("forkPath", "child") ? FORK_CHILD
                       : res_is("forkPath", "both") ? FORK_BOTH : FORK_PARENT;
    if (dbxl_res_str("generalRegisters"))
        dbxl_opt.reg[REG_GENERAL] = !res_is("generalRegisters", "hide");
    if (dbxl_res_str("doubleRegisters"))
        dbxl_opt.reg[REG_DOUBLE] = res_is("doubleRegisters", "show");
    if (dbxl_res_str("specialRegisters"))
        dbxl_opt.reg[REG_SPECIAL] = !res_is("specialRegisters", "hide");

    dbxl_format_defaults();
    dbxl_fmt.justify = res_is("scalarJustification", "left") ? JUSTIFY_LEFT
                     : res_is("scalarJustification", "right") ? JUSTIFY_RIGHT
                     : res_is("scalarJustification", "center") ? JUSTIFY_CENTER
                     : JUSTIFY_NONE;
    dbxl_fmt.max_array = dbxl_res_int("maxArrayElements", 1000);
    dbxl_fmt.max_string = dbxl_res_int("maxString", 200);
    dbxl_parse_styles(dbxl_res_str("signedIntegerStyles"), dbxl_fmt.signed_style, 4);
    dbxl_parse_styles(dbxl_res_str("unsignedIntegerStyles"),
                      dbxl_fmt.unsigned_style, 4);
    dbxl_parse_styles(dbxl_res_str("floatStyles"), dbxl_fmt.float_style, 3);
    if ((s = dbxl_res_str("floatDigits"))) {
        for (int i = 0; i < 3 && *s; i++) {
            char *end;
            long d;

            s += strspn(s, " \t");
            d = strtol(s, &end, 10);
            if (end != s && d > 0 && d < 40)
                dbxl_fmt.float_digits[i] = (int)d;
            s = end != s ? end : s + strcspn(s, " \t");
        }
    }
}

/* ignoreSignals and -i: numbers or names, with or without SIG. */
static int ignored_signals(const struct dbxl_opts *o, char names[][16], int max)
{
    const char *list = dbxl_res_str("ignoreSignals");
    char word[32];
    int n = 0;

    while (list && *list && n < max) {
        size_t len;

        list += strspn(list, " \t,");
        len = strcspn(list, " \t,");
        if (!len)
            break;
        snprintf(word, sizeof word, "%.*s", (int)len, list);
        list += len;
        if (dbg_signal_name(word, names[n], 16))
            n++;
        else
            fprintf(stderr, "dbxl: unknown signal %s\n", word);
    }
    for (int i = 0; i < o->nignore && n < max; i++) {
        if (dbg_signal_name(o->ignore[i], names[n], 16))
            n++;
        else
            fprintf(stderr, "dbxl: unknown signal %s\n", o->ignore[i]);
    }
    return n;
}

int main(int argc, char **argv)
{
    struct dbxl_opts o;
    struct dbxl_ui_config ucfg;
    struct dbxl_debug_config dcfg;
    xtk_config cfg;
    char title[256], ignore[64][16];
    const char *geometry;
    xtk_rect geom;
    int status, scr;

    if (dbxl_opts_parse(&o, argc, argv) < 0)
        return 255;
    if (o.help) {
        dbxl_help_print(stdout);
        return 0;
    }
    if (!xtk_open(o.display))
        return 1;
    dbxl_res_init(xtk_dpy(), o.name ? o.name : dbxl_res_default_name(argv[0]));
    dbxl_options_defaults();
    apply_resources();
    if (o.max_array > 0)
        dbxl_fmt.max_array = o.max_array;
    if (o.core && !o.program) {
        fprintf(stderr, "dbxl: -co needs the program that produced the core\n");
        return 255;
    }
    if (o.core && access("core", R_OK) != 0) {
        /* xldb: M_cannot_open_core_file, then exit 1 (recon pass 17). */
        fprintf(stderr, "dbxl: Cannot open the core file:  errno=%d: %s\n",
                errno, strerror(errno));
        return 1;
    }

    memset(&cfg, 0, sizeof cfg);
    cfg.font = o.font ? o.font : dbxl_res_str("font");
    if (!cfg.font)
        cfg.font = "8x13";
    cfg.scale = o.scale >= 0 ? o.scale : dbxl_res_int("scale", 1);
    if (o.scheme >= 0)
        cfg.scheme = (enum xtk_scheme)o.scheme;
    else if (dbxl_res_bool("blackOnWhite", false))
        cfg.scheme = XTK_SCHEME_BW;
    else if (dbxl_res_bool("whiteOnBlack", false))
        cfg.scheme = XTK_SCHEME_WB;
    for (size_t i = 0; i < sizeof color_resources / sizeof color_resources[0]; i++)
        cfg.color[color_resources[i].role] =
            dbxl_res_str(color_resources[i].resource);
    if (o.bg)
        cfg.color[XTK_BG] = o.bg;
    if (o.fg)
        cfg.color[XTK_FG] = o.fg;

    geometry = o.geometry ? o.geometry : dbxl_res_str("geometry");
    geom = frame_geometry(geometry, 0, 0);
    cfg.fit_w = geom.w + 4;
    cfg.fit_h = geom.h + 4;
    if (!xtk_setup(&cfg))
        return 1;
    scr = DefaultScreen(xtk_dpy());
    geom = frame_geometry(geometry, DisplayWidth(xtk_dpy(), scr) / xtk_scale(),
                          DisplayHeight(xtk_dpy(), scr) / xtk_scale());

    if (o.title) {
        snprintf(title, sizeof title, "%s", o.title);
    } else if (o.program) {
        const char *base = strrchr(o.program, '/');
        snprintf(title, sizeof title, "dbxl %s", base ? base + 1 : o.program);
    } else {
        snprintf(title, sizeof title, "dbxl");
    }
    xtk_frame_create(geom, title, "dbxl", argc, argv);

    dbxl_opt.autoraise = dbxl_res_bool("autoRaise", true);
    ucfg.tag_windows = dbxl_res_bool("tagWindows", false);
    ucfg.confirm_exit = dbxl_res_bool("confirmExit", true);
    ucfg.command_list = dbxl_res_str("commandList");
    if (o.core)
        /* Core files: only these, without Alt keys (recon pass 17). */
        ucfg.command_list = "\"Edit             \"=edit \"Exit             \"=exit "
                            "\"Options          \"=options "
                            "\"Help             \"=help";
    dbxl_ui_init(&ucfg);

    wm_protocols = XInternAtom(xtk_dpy(), "WM_PROTOCOLS", False);
    wm_delete = XInternAtom(xtk_dpy(), "WM_DELETE_WINDOW", False);
    xtk_loop_add_handler(close_event, NULL);

    /* -q: the window appears at the first stop (a signal). */
    if (!o.quiet)
        XMapWindow(xtk_dpy(), xtk_frame());

    memset(&dcfg, 0, sizeof dcfg);
    dcfg.program = o.program;
    dcfg.argc = o.prog_argc;
    dcfg.argv = o.prog_argv;
    dcfg.run_to = o.run_to ? o.run_to : dbxl_res_str("runTo");
    dcfg.quiet = o.quiet;
    dcfg.core = o.core ? "core" : NULL;
    dcfg.attach_pid = o.attach_pid;
    dcfg.max_calls = o.max_calls > 0 ? o.max_calls : dbxl_res_int("maxCalls", 100);
    if (dcfg.max_calls < 1)
        dcfg.max_calls = 1;
    dcfg.nignore = ignored_signals(&o, ignore, 64);
    dcfg.ignore = ignore;
    dcfg.no_shared = o.no_shared || dbxl_res_bool("ignoreSharedObjects", false);
    dcfg.verbose = o.verbose;
    dcfg.fetch_source = o.fetch ? o.fetch : dbxl_res_str("fetchSource");
    dcfg.automatic_breakpoints = dbxl_res_bool("automaticBreakpoints", false);
    dcfg.edit = o.edit ? o.edit : dbxl_res_str("edit");
    dcfg.include = o.include;
    dcfg.ninclude = o.ninclude;
    dcfg.breakpoints_file = dbxl_res_str("breakpointsFile");
    dcfg.load_breakpoints = dbxl_res_bool("loadBreakpoints", false) &&
                            !o.no_load_breakpoints;
    dbxl_debug_init(&dcfg);

    status = xtk_loop_run();
    /* xldb's return code (the help): the program's, if it finished without
     * a runtime error; 1 if xldb failed; otherwise 255. */
    if (status == 0)
        status = dbxl_debug_exit_status();
    dbxl_debug_shutdown();
    dbxl_res_free();
    xtk_close();
    return status;
}
