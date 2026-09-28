/*
 * Command-line options.  Milestone 1 handles the options that affect the
 * window; the rest of xldb's option set (DESIGN.md 3) follows with the
 * features they control.
 */
#include <stdio.h>
#include <string.h>

#include "opts.h"

static void usage(void)
{
    fprintf(stderr,
            "usage: dbxl [-display Display] [-font Font] [-geometry Geometry]\n"
            "            [-title Title] [Program [ProgramArgument...]]\n");
}

int dbxl_opts_parse(struct dbxl_opts *o, int argc, char **argv)
{
    int i;

    memset(o, 0, sizeof *o);
    o->font = "8x13";
    for (i = 1; i < argc && argv[i][0] == '-'; i++) {
        const char *a = argv[i];
        const char **dst = NULL;

        if (strcmp(a, "-display") == 0)
            dst = &o->display;
        else if (strcmp(a, "-font") == 0)
            dst = &o->font;
        else if (strcmp(a, "-geometry") == 0)
            dst = &o->geometry;
        else if (strcmp(a, "-title") == 0)
            dst = &o->title;
        else {
            fprintf(stderr, "dbxl: unknown option %s\n", a);
            usage();
            return -1;
        }
        if (++i >= argc) {
            fprintf(stderr, "dbxl: option %s needs an argument\n", a);
            usage();
            return -1;
        }
        *dst = argv[i];
    }
    if (i < argc) {
        o->program = argv[i];
        o->prog_argc = argc - i - 1;
        o->prog_argv = argv + i + 1;
    }
    return 0;
}
