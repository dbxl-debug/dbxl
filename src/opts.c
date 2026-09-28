/*
 * Command-line options (xldb's set, DESIGN.md 3); the options for features
 * not implemented yet are added with those features.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "opts.h"
#include "xtk/xtk.h"

static void usage(void)
{
    fprintf(stderr,
            "usage: dbxl [-display Display] [-name Name] [-font Font]\n"
            "            [-geometry Geometry] [-title Title]\n"
            "            [-bg Color] [-fg Color] [-bw] [-wb]\n"
            "            [-scale auto|1|2|3|4] [-E Command] [-F Command]\n"
            "            [-I Directory] [-a ProcessId] [-c MaxCalls] [-co]\n"
            "            [-e MaxArrayElements] [-i Signal] [-k] [-n] [-q]\n"
            "            [-r SubprogramName] [-v] [-h]\n"
            "            [Program [ProgramArgument...]]\n");
}

int dbxl_opts_parse(struct dbxl_opts *o, int argc, char **argv)
{
    int i;

    memset(o, 0, sizeof *o);
    o->scheme = -1;
    o->scale = -1;
    o->max_calls = -1;
    o->max_array = -1;
    for (i = 1; i < argc && argv[i][0] == '-'; i++) {
        const char *a = argv[i];
        const char **dst = NULL;

        if (strcmp(a, "-bw") == 0) {
            o->scheme = XTK_SCHEME_BW;          /* the later of -bw/-wb wins */
            continue;
        }
        if (strcmp(a, "-wb") == 0) {
            o->scheme = XTK_SCHEME_WB;
            continue;
        }
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
        if (strcmp(a, "-h") == 0) {
            o->help = 1;
            continue;
        }
        if (strcmp(a, "-k") == 0) {
            o->no_load_breakpoints = 1;
            continue;
        }
        if (strcmp(a, "-co") == 0) {
            o->core = 1;
            continue;
        }
        if (strcmp(a, "-n") == 0) {
            o->no_shared = 1;
            continue;
        }
        if (strcmp(a, "-q") == 0) {
            o->quiet = 1;
            continue;
        }
        if (strcmp(a, "-v") == 0) {
            o->verbose = 1;
            continue;
        }
        if (strcmp(a, "-a") == 0 || strcmp(a, "-c") == 0 || strcmp(a, "-e") == 0) {
            char *end;
            long n;

            if (++i >= argc) {
                fprintf(stderr, "dbxl: %s option requires an argument\n", a);
                usage();
                return -1;
            }
            n = strtol(argv[i], &end, 10);
            if (*end || n <= 0) {
                fprintf(stderr, "dbxl: %s needs a positive number\n", a);
                usage();
                return -1;
            }
            if (a[1] == 'a')
                o->attach_pid = (int)n;
            else if (a[1] == 'c')
                o->max_calls = (int)n;
            else
                o->max_array = (int)n;
            continue;
        }
        if (strcmp(a, "-i") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "dbxl: -i option requires an argument\n");
                usage();
                return -1;
            }
            if (o->nignore < (int)(sizeof o->ignore / sizeof o->ignore[0]))
                o->ignore[o->nignore++] = argv[i];
            continue;
        }
        if (strcmp(a, "-I") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "dbxl: -I option requires an argument\n");
                usage();
                return -1;
            }
            if (o->ninclude < (int)(sizeof o->include / sizeof o->include[0]))
                o->include[o->ninclude++] = argv[i];
            continue;
        }
        if (strcmp(a, "-display") == 0)
            dst = &o->display;
        else if (strcmp(a, "-name") == 0)
            dst = &o->name;
        else if (strcmp(a, "-font") == 0)
            dst = &o->font;
        else if (strcmp(a, "-geometry") == 0)
            dst = &o->geometry;
        else if (strcmp(a, "-title") == 0)
            dst = &o->title;
        else if (strcmp(a, "-bg") == 0)
            dst = &o->bg;
        else if (strcmp(a, "-fg") == 0)
            dst = &o->fg;
        else if (strcmp(a, "-E") == 0)
            dst = &o->edit;
        else if (strcmp(a, "-F") == 0)
            dst = &o->fetch;
        else if (strcmp(a, "-r") == 0)
            dst = &o->run_to;
        else {
            fprintf(stderr, "dbxl: unknown option %s\n", a);
            usage();
            return -1;
        }
        if (++i >= argc) {
            fprintf(stderr, "dbxl: %s option requires an argument\n", a);
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
