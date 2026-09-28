#include <stdio.h>
#include <string.h>

#include "core/options.h"

struct dbxl_options dbxl_opt;

void dbxl_options_defaults(void)
{
    memset(&dbxl_opt, 0, sizeof dbxl_opt);
    dbxl_opt.detail_per_click = 1;
    dbxl_opt.multiprocess = true;
    dbxl_opt.autoraise = true;
    /* Special registers are shown by default (the help's "Hide" is wrong). */
    dbxl_opt.reg[REG_GENERAL] = true;
    dbxl_opt.reg[REG_SPECIAL] = true;
    snprintf(dbxl_opt.source_path, sizeof dbxl_opt.source_path, ". %%o ");
}

/*
 * " label<spaces>value " with label + spaces + value exactly `inner`
 * columns (at least 4 spaces), as xldb draws them:
 *   " Local variables:        Active "
 *   " Breakpoint all Subprograms    no "
 */
static void fmt(char *buf, int size, int inner, const char *label,
                const char *value)
{
    int ll = (int)strlen(label), vl = value ? (int)strlen(value) : 0;
    int gap = inner - ll - vl;

    if (!value) {
        snprintf(buf, (size_t)size, " %-*s ", inner, label);
        return;
    }
    if (gap < 4)
        gap = 4;
    snprintf(buf, (size_t)size, " %s%*s%s ", label, gap, "", value);
}

void dbxl_options_item(int k, char *buf, int size)
{
    const struct dbxl_options *o = &dbxl_opt;
    char n[16];

    switch (k) {
    case OPT_SAVE_LAYOUT:
        fmt(buf, size, 30, "Save layout", NULL); break;
    case OPT_SOURCE_PATH:
        fmt(buf, size, 30, "Edit source search path", NULL); break;
    case OPT_LOCAL_VARIABLES:
        fmt(buf, size, 30, "Local variables:", o->local_all ? "All" : "Active");
        break;
    case OPT_DETAIL_PER_CLICK:
        snprintf(n, sizeof n, "%d", o->detail_per_click);
        fmt(buf, size, 30, "Detail per click:", n);
        break;
    case OPT_MULTIPROCESS:
        fmt(buf, size, 30, "Multiprocess debugging", o->multiprocess ? "yes" : "no");
        break;
    case OPT_FORK_PATH:
        fmt(buf, size, 30, "Fork path:", o->fork_child ? "Child" : "Parent");
        break;
    case OPT_AUTORAISE:
        fmt(buf, size, 30, "Autoraise:", o->autoraise ? "yes" : "no"); break;
    case OPT_COMPILER_VARIABLES:
        fmt(buf, size, 30, "Compiler variables:", o->compiler_vars ? "Show" : "Hide");
        break;
    case OPT_LANGUAGE:
        fmt(buf, size, 30, "Language:", "Automatic"); break;
    case OPT_REGISTER_CONTROL:
        fmt(buf, size, 30, "Register control", NULL); break;
    case OPT_SUBPROGRAMS:
        fmt(buf, size, 30, "Subprograms:", o->subprograms_all ? "All" : "With -g  only");
        break;
    case OPT_SAVE_BREAKPOINTS:
        fmt(buf, size, 30, "Save Breakpoints", NULL); break;
    case OPT_LOAD_BREAKPOINTS:
        fmt(buf, size, 30, "Load Breakpoints", NULL); break;
    case OPT_CASE_SENSITIVE:
        fmt(buf, size, 30, "Case Sensitive", o->case_sensitive ? "yes" : "no");
        break;
    case OPT_BREAK_ALL:
        fmt(buf, size, 30, "Breakpoint all Subprograms", o->break_all ? "yes" : "no");
        break;
    default:
        buf[0] = '\0';
    }
}

void dbxl_options_register_item(int k, char *buf, int size)
{
    static const char *const label[REG_NITEMS] = {
        "General registers:", "Double registers:", "Special registers:"
    };

    fmt(buf, size, 31, label[k], dbxl_opt.reg[k] ? "Show" : "Hide");
}

bool dbxl_options_cycle(int k, bool forward)
{
    struct dbxl_options *o = &dbxl_opt;

    switch (k) {
    case OPT_LOCAL_VARIABLES:    o->local_all = !o->local_all; return true;
    case OPT_MULTIPROCESS:       o->multiprocess = !o->multiprocess; return true;
    case OPT_FORK_PATH:          o->fork_child = !o->fork_child; return true;
    case OPT_AUTORAISE:          o->autoraise = !o->autoraise; return true;
    case OPT_COMPILER_VARIABLES: o->compiler_vars = !o->compiler_vars; return true;
    case OPT_SUBPROGRAMS:        o->subprograms_all = !o->subprograms_all; return true;
    case OPT_CASE_SENSITIVE:     o->case_sensitive = !o->case_sensitive; return true;
    case OPT_BREAK_ALL:          o->break_all = !o->break_all; return true;
    case OPT_DETAIL_PER_CLICK:
        /* The help: left-click decreases, right-click increases. */
        if (forward && o->detail_per_click > 1)
            o->detail_per_click--;
        else if (!forward)
            o->detail_per_click++;
        return true;
    }
    return false;
}

void dbxl_options_register_cycle(int k)
{
    dbxl_opt.reg[k] = !dbxl_opt.reg[k];
}
