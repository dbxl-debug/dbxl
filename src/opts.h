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
    const char *include[32]; /* -I: source directories */
    int ninclude;
    const char *program;     /* program to debug */
    int prog_argc;           /* its arguments */
    char **prog_argv;
};

/* Parse argv.  Returns 0 on success, prints usage and returns -1 on error. */
int dbxl_opts_parse(struct dbxl_opts *o, int argc, char **argv);

#endif
