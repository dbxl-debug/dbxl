#ifndef DBXL_OPTS_H
#define DBXL_OPTS_H

/*
 * Command-line options.  Values left NULL / -1 fall back to resources and
 * then to xldb's defaults.
 */
struct dbxl_opts {
    const char *display;     /* -display */
    const char *name;        /* -name: resource name */
    const char *font;        /* -font */
    const char *geometry;    /* -geometry */
    const char *title;       /* -title */
    const char *bg, *fg;     /* -bg, -fg */
    int scheme;              /* -1 unset, else enum xtk_scheme (-bw, -wb) */
    int scale;               /* -1 unset, 0 = auto, 1-4 */
    const char *edit;        /* -E: edit command */
    int no_load_breakpoints; /* -k: don't load saved breakpoints */
    int help;                /* -h: print the help text and exit */
    const char *include[32]; /* -I: source directories */
    int ninclude;
    int attach_pid;          /* -a ProcessId */
    int core;                /* -co: debug ./core */
    int max_calls;           /* -c: -1 unset */
    int max_array;           /* -e: -1 unset */
    const char *fetch;       /* -F: fetch-source command */
    const char *ignore[32];  /* -i: signals to pass without stopping */
    int nignore;
    int no_shared;           /* -n: ignore shared objects */
    int quiet;               /* -q: appear only at a signal */
    const char *run_to;      /* -r: first stop */
    int verbose;             /* -v */
    const char *program;     /* program to debug */
    int prog_argc;           /* its arguments */
    char **prog_argv;
};

/* Parse argv.  Returns 0 on success, prints usage and returns -1 on error. */
int dbxl_opts_parse(struct dbxl_opts *o, int argc, char **argv);

#endif
