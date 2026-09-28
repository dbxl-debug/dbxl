#ifndef DBXL_OPTS_H
#define DBXL_OPTS_H

struct dbxl_opts {
    const char *display;     /* -display */
    const char *font;        /* -font */
    const char *geometry;    /* -geometry */
    const char *title;       /* -title */
    int scale;               /* -scale: 1-4, or 0 = auto */
    const char *program;     /* program to debug */
    int prog_argc;           /* its arguments */
    char **prog_argv;
};

/* Parse argv.  Returns 0 on success, prints usage and returns -1 on error. */
int dbxl_opts_parse(struct dbxl_opts *o, int argc, char **argv);

#endif
