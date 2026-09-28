/*
 * Command-line options.  Milestone 1 handles the options that affect the
 * window; the rest of xldb's option set (DESIGN.md 3) follows with the
 * features they control.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "opts.h"

static void usage(void)
{
    fprintf(stderr,
            "usage: dbxl [-display Display] [-font Font] [-geometry Geometry]\n"
            "            [-scale auto|1|2|3|4] [-title Title]\n"
            "            [Program [ProgramArgument...]]\n");
}

int dbxl_opts_parse(struct dbxl_opts *o, int argc, char **argv)
{
    int i;

    memset(o, 0, sizeof *o);
    o->font = "8x13";
    o->scale = 1;
    for (i = 1; i < argc && argv[i][0] == '-'; i++) {
        const char *a = argv[i];
        const char **dst = NULL;

        if (strcmp(a, "-scale") == 0) {
            const char *v = ++i < argc ? argv[i] : NULL;
            if (v && strcmp(v, "auto") == 0) {
                o->scale = 0;
            } else if (v && v[0] >= '1' && v[0] <= '4' && v[1] == '\0') {
                o->scale = v[0] - '0';
            } else {
                fprintf(stderr, "dbxl: -scale must be auto, 1, 2, 3 or 4\n");
                usage();
                return -1;
            }
            continue;
        }
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
