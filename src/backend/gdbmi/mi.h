/*
 * GDB/MI output records (GDB manual, "GDB/MI Output Syntax").
 *
 *   [token] ^ result-class ( , result )*        result record
 *   [token] * async-class ( , result )*         exec async  (*stopped)
 *   [token] + async-class ...                   status async
 *   [token] = async-class ...                   notify async
 *   ~ "text"  @ "text"  & "text"                stream records
 *   (gdb)                                       prompt
 *
 * A value is a C string, a tuple {name=value,...} or a list [value,...] /
 * [name=value,...].
 */
#ifndef DBXL_MI_H
#define DBXL_MI_H

enum mi_kind { MI_CONST, MI_TUPLE, MI_LIST };

typedef struct mi_value {
    enum mi_kind kind;
    char *name;                  /* when this value is a result (name=value) */
    char *str;                   /* MI_CONST */
    struct mi_value *child;      /* MI_TUPLE / MI_LIST: first element */
    struct mi_value *next;       /* next sibling */
} mi_value;

enum mi_record_type {
    MI_RESULT,                   /* ^ */
    MI_EXEC,                     /* * */
    MI_STATUS,                   /* + */
    MI_NOTIFY,                   /* = */
    MI_CONSOLE,                  /* ~ */
    MI_TARGET,                   /* @ */
    MI_LOG,                      /* & */
    MI_PROMPT,                   /* (gdb) */
    MI_UNKNOWN
};

typedef struct mi_record {
    enum mi_record_type type;
    long token;                  /* -1 if none */
    char *klass;                 /* done, running, stopped, ... */
    char *text;                  /* stream records: decoded text */
    mi_value *results;           /* tuple of the results (may be NULL) */
} mi_record;

/* Parse one line of GDB output.  Returns NULL on a malformed line. */
mi_record *mi_parse(const char *line);
void mi_record_free(mi_record *r);

/* Lookups.  "a.b" walks nested tuples. NULL / dflt when absent. */
const mi_value *mi_get(const mi_value *tuple, const char *path);
const char *mi_str(const mi_value *tuple, const char *path);
long mi_int(const mi_value *tuple, const char *path, long dflt);

/* Quote a string as an MI C-string argument ("..." with escapes). */
void mi_quote(const char *s, char *out, int size);

#endif
