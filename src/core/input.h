/*
 * Values typed into Edit (and trigger) dialogs, read in the object's
 * display style as xldb's help describes, with xldb's hints and range
 * messages (recon pass 15; the catalogue, docs/RECON.md).
 */
#ifndef DBXL_INPUT_H
#define DBXL_INPUT_H

#include <stdbool.h>
#include <stddef.h>

#include "backend/backend.h"
#include "core/value.h"

typedef struct dbxl_input {
    char literal[300];           /* for the debugger: "-5", "0x62", "1.5e3" */
    bool bits;                   /* the literal is the raw bits (float hex) */
} dbxl_input;

/*
 * Parse text for value v shown in style st (STYLE_DEFAULT: the kind's own).
 * Returns 0 and fills in; or -1 with xldb's message in msg.
 */
int dbxl_parse_input(const dbg_value *v, enum dbxl_style st, const char *text,
                     dbxl_input *in, char *msg, size_t msgsize);

#endif
