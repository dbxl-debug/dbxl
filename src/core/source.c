/*
 * Source files (DESIGN.md 8: Source).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "core/source.h"

#define MAX_CACHED 32

static dbxl_source *cache[MAX_CACHED];
static int ncached;

static int is_file(const char *path)
{
    struct stat st;

    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static const char *base_name(const char *path)
{
    const char *s = strrchr(path, '/');

    return s ? s + 1 : path;
}

int dbxl_source_find(const char *file, const char *fullname,
                     const char *search_path, const char *program,
                     char *out, size_t size)
{
    char item[1024], dir[1024];
    const char *p = search_path ? search_path : ". %o";

    if (file && *file == '/' && is_file(file)) {
        snprintf(out, size, "%s", file);
        return 0;
    }
    while (file && *file && *p) {
        size_t n = strcspn(p, " \t");

        if (n > 0 && n < sizeof item) {
            memcpy(item, p, n);
            item[n] = '\0';
            if (strcmp(item, "%o") == 0) {
                const char *slash = program ? strrchr(program, '/') : NULL;
                if (slash)
                    snprintf(dir, sizeof dir, "%.*s", (int)(slash - program),
                             program);
                else
                    snprintf(dir, sizeof dir, ".");
            } else {
                snprintf(dir, sizeof dir, "%s", item);
            }
            /* Try the name as compiled, then its last component. */
            snprintf(out, size, "%s/%s", dir, file);
            if (is_file(out))
                return 0;
            snprintf(out, size, "%s/%s", dir, base_name(file));
            if (is_file(out))
                return 0;
        }
        p += n;
        p += strspn(p, " \t");
    }
    if (fullname && *fullname && is_file(fullname)) {
        snprintf(out, size, "%s", fullname);
        return 0;
    }
    return -1;
}

/* One line with tabs expanded to 8-column stops. */
static char *expand(const char *s, size_t len)
{
    size_t cap = len + 1, n = 0;
    char *out;

    for (size_t i = 0; i < len; i++)
        if (s[i] == '\t')
            cap += 7;
    out = malloc(cap);
    for (size_t i = 0; i < len; i++) {
        if (s[i] == '\t') {
            do
                out[n++] = ' ';
            while (n % 8);
        } else if (s[i] != '\r') {
            out[n++] = s[i];
        }
    }
    out[n] = '\0';
    return out;
}

const dbxl_source *dbxl_source_load(const char *path)
{
    FILE *f;
    char *data = NULL;
    size_t len = 0, cap = 0;
    dbxl_source *src;
    int nalloc = 64;

    for (int i = 0; i < ncached; i++)
        if (strcmp(cache[i]->path, path) == 0)
            return cache[i];
    f = fopen(path, "r");
    if (!f)
        return NULL;
    for (;;) {
        size_t n;
        if (len + 4096 > cap) {
            cap = (cap + 4096) * 2;
            data = realloc(data, cap);
        }
        n = fread(data + len, 1, cap - len, f);
        if (n == 0)
            break;
        len += n;
    }
    fclose(f);

    src = calloc(1, sizeof *src);
    src->path = strdup(path);
    src->lines = malloc((size_t)nalloc * sizeof *src->lines);
    for (size_t start = 0; start < len;) {
        size_t end = start;
        while (end < len && data[end] != '\n')
            end++;
        if (src->nlines == nalloc) {
            nalloc *= 2;
            src->lines = realloc(src->lines, (size_t)nalloc * sizeof *src->lines);
        }
        src->lines[src->nlines++] = expand(data + start, end - start);
        start = end + 1;
    }
    free(data);

    if (ncached == MAX_CACHED) {
        dbxl_source *old = cache[0];
        for (int i = 0; i < old->nlines; i++)
            free(old->lines[i]);
        free(old->lines);
        free(old->path);
        free(old);
        memmove(cache, cache + 1, (MAX_CACHED - 1) * sizeof cache[0]);
        ncached--;
    }
    cache[ncached++] = src;
    return src;
}
