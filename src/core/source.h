/*
 * Source files: finding them on xldb's source search path and loading
 * them as display lines.
 */
#ifndef DBXL_SOURCE_H
#define DBXL_SOURCE_H

#include <stddef.h>

typedef struct dbxl_source {
    char *path;                  /* as found, e.g. "./test.c" (the title) */
    char **lines;                /* tabs expanded, no newlines */
    int nlines;
} dbxl_source;

/*
 * Find `file` (as compiled) on the search path: space-separated items,
 * each a directory or "%o" (the directory of the program).  Falls back to
 * `fullname` (the debugger's resolved path) when it exists.  Writes the
 * path to open into out; returns 0 when found.
 */
int dbxl_source_find(const char *file, const char *fullname,
                     const char *search_path, const char *program,
                     char *out, size_t size);

/* Load (cached by path).  NULL when it cannot be read. */
const dbxl_source *dbxl_source_load(const char *path);

#endif
