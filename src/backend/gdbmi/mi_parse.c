/*
 * GDB/MI output parser: one line in, one record out.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "backend/gdbmi/mi.h"

static mi_value *new_value(enum mi_kind kind)
{
    mi_value *v = calloc(1, sizeof *v);

    v->kind = kind;
    return v;
}

static void free_value(mi_value *v)
{
    while (v) {
        mi_value *next = v->next;

        free(v->name);
        free(v->str);
        free_value(v->child);
        free(v);
        v = next;
    }
}

/* A C string "..." with backslash escapes; *pp points at the quote. */
static char *cstring(const char **pp)
{
    const char *p = *pp + 1;
    size_t cap = 64, n = 0;
    char *out = malloc(cap);

    while (*p && *p != '"') {
        char c = *p++;

        if (c == '\\' && *p) {
            c = *p++;
            switch (c) {
            case 'n': c = '\n'; break;
            case 't': c = '\t'; break;
            case 'r': c = '\r'; break;
            case 'a': c = '\a'; break;
            case 'b': c = '\b'; break;
            case 'f': c = '\f'; break;
            case 'v': c = '\v'; break;
            case 'e': c = 27; break;
            default:
                if (c >= '0' && c <= '7') {       /* \ooo */
                    int v = c - '0';
                    for (int k = 0; k < 2 && *p >= '0' && *p <= '7'; k++)
                        v = v * 8 + (*p++ - '0');
                    c = (char)v;
                }
            }
        }
        if (n + 2 > cap)
            out = realloc(out, cap *= 2);
        out[n++] = c;
    }
    out[n] = '\0';
    *pp = *p == '"' ? p + 1 : p;
    return out;
}

static mi_value *value(const char **pp);

/* name=value, or a bare value inside a list. */
static mi_value *result_or_value(const char **pp)
{
    const char *p = *pp;
    const char *start = p;

    while (*p && (isalnum((unsigned char)*p) || *p == '-' || *p == '_'))
        p++;
    if (p > start && *p == '=') {
        char *name = strndup(start, (size_t)(p - start));
        mi_value *v;

        p++;
        v = value(&p);
        if (!v) {
            free(name);
            return NULL;
        }
        v->name = name;
        *pp = p;
        return v;
    }
    return value(pp);
}

/* Elements up to `close`, separated by commas. */
static mi_value *elements(const char **pp, char close, enum mi_kind kind)
{
    const char *p = *pp + 1;
    mi_value *v = new_value(kind), **tail = &v->child;

    while (*p && *p != close) {
        mi_value *e = result_or_value(&p);

        if (!e) {
            free_value(v);
            return NULL;
        }
        *tail = e;
        tail = &e->next;
        if (*p == ',')
            p++;
    }
    if (*p != close) {
        free_value(v);
        return NULL;
    }
    *pp = p + 1;
    return v;
}

static mi_value *value(const char **pp)
{
    const char *p = *pp;

    if (*p == '"') {
        mi_value *v = new_value(MI_CONST);
        v->str = cstring(pp);
        return v;
    }
    if (*p == '{')
        return elements(pp, '}', MI_TUPLE);
    if (*p == '[')
        return elements(pp, ']', MI_LIST);
    return NULL;
}

mi_record *mi_parse(const char *line)
{
    const char *p = line;
    mi_record *r = calloc(1, sizeof *r);
    const char *start;

    r->token = -1;
    if (strncmp(line, "(gdb)", 5) == 0) {
        r->type = MI_PROMPT;
        return r;
    }
    if (isdigit((unsigned char)*p)) {
        r->token = strtol(p, (char **)&p, 10);
    }
    switch (*p) {
    case '^': r->type = MI_RESULT; break;
    case '*': r->type = MI_EXEC; break;
    case '+': r->type = MI_STATUS; break;
    case '=': r->type = MI_NOTIFY; break;
    case '~': r->type = MI_CONSOLE; break;
    case '@': r->type = MI_TARGET; break;
    case '&': r->type = MI_LOG; break;
    default:
        r->type = MI_UNKNOWN;
        r->text = strdup(line);
        return r;
    }
    p++;
    if (r->type >= MI_CONSOLE) {
        if (*p == '"')
            r->text = cstring(&p);
        else
            r->text = strdup(p);
        return r;
    }
    start = p;
    while (*p && *p != ',')
        p++;
    r->klass = strndup(start, (size_t)(p - start));
    r->results = new_value(MI_TUPLE);
    {
        mi_value **tail = &r->results->child;

        while (*p == ',') {
            mi_value *e;

            p++;
            e = result_or_value(&p);
            if (!e) {
                mi_record_free(r);
                return NULL;
            }
            *tail = e;
            tail = &e->next;
        }
    }
    return r;
}

void mi_record_free(mi_record *r)
{
    if (!r)
        return;
    free(r->klass);
    free(r->text);
    free_value(r->results);
    free(r);
}

const mi_value *mi_get(const mi_value *v, const char *path)
{
    char part[64];

    while (v && *path) {
        const char *dot = strchr(path, '.');
        size_t n = dot ? (size_t)(dot - path) : strlen(path);
        const mi_value *c;

        if (n >= sizeof part)
            return NULL;
        memcpy(part, path, n);
        part[n] = '\0';
        for (c = v->child; c; c = c->next)
            if (c->name && strcmp(c->name, part) == 0)
                break;
        v = c;
        path += n + (dot ? 1 : 0);
    }
    return v;
}

const char *mi_str(const mi_value *v, const char *path)
{
    const mi_value *c = mi_get(v, path);

    return c && c->kind == MI_CONST ? c->str : NULL;
}

long mi_int(const mi_value *v, const char *path, long dflt)
{
    const char *s = mi_str(v, path);
    char *end;
    long n;

    if (!s)
        return dflt;
    n = strtol(s, &end, 0);
    return end == s ? dflt : n;
}

void mi_quote(const char *s, char *out, int size)
{
    int n = 0;

    if (size < 3)
        return;
    out[n++] = '"';
    for (; *s && n < size - 3; s++) {
        if (*s == '"' || *s == '\\')
            out[n++] = '\\';
        out[n++] = *s;
    }
    out[n++] = '"';
    out[n] = '\0';
}
