/*
 * Typed values, read by display style (xldb's help, "Changing ...";
 * messages from xldb's catalogue).
 */
#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "core/input.h"

static const char *const HINT_SIGNED =
    "Enter a signed decimal number (examples: +123, -65091, 12).";
static const char *const HINT_UNSIGNED =
    "Enter an unsigned decimal number (examples: 123, 65091, 12).";
static const char *const HINT_HEX =
    "Enter an unsigned hex number (examples: 0x12,  5a, 13, deadbeef, 0x5465).";
static const char *const HINT_CHAR =
    "Enter a quoted character (examples: 'a', ' ', '\"', ''', '\\n', '\\x12').";
static const char *const HINT_LOGICAL =
    "Enter true, false, or a signed decimal number (examples: true, -12, f, "
    "+17, tr, 5).";

static void trim(const char *text, char *out, size_t size)
{
    size_t n;

    while (isspace((unsigned char)*text))
        text++;
    snprintf(out, size, "%s", text);
    n = strlen(out);
    while (n > 0 && isspace((unsigned char)out[n - 1]))
        out[--n] = '\0';
}

static bool all_digits(const char *s, int (*ok)(int))
{
    if (!*s)
        return false;
    for (; *s; s++)
        if (!ok((unsigned char)*s))
            return false;
    return true;
}

/* xldb's range messages per size (M_<n>_byte_*_integer_out_of_range). */
static void range_message(int size, enum dbxl_style st, bool is_signed,
                          char *msg, size_t n)
{
    int bits = size * 8;
    uint64_t umax = bits >= 64 ? UINT64_MAX : (UINT64_C(1) << bits) - 1;
    int64_t smax = (int64_t)(umax >> 1), smin = -smax - 1;

    if (st == STYLE_HEX) {
        if (size >= 8)
            snprintf(msg, n, "Enter a value in 0x%016" PRIx64 " .. 0x%016" PRIx64 ".",
                     (uint64_t)0, umax);
        else
            snprintf(msg, n, "Enter a value in 0x%0*x .. 0x%0*" PRIx64 ".",
                     size * 2, 0, size * 2, umax);
    } else if (is_signed) {
        snprintf(msg, n, "Enter a value in %" PRId64 " .. %" PRId64 ".", smin, smax);
    } else {
        snprintf(msg, n, "Enter a value in %u .. %" PRIu64 ".", 0, umax);
    }
}

/* A quoted character with C escapes ('a', '\n', '\x12', '\101'). */
static int parse_char(const char *t, unsigned *c)
{
    size_t n = strlen(t);

    if (n < 3 || t[0] != '\'' || t[n - 1] != '\'')
        return -1;
    if (n == 3) {
        *c = (unsigned char)t[1];
        return 0;
    }
    if (t[1] != '\\')
        return -1;
    switch (t[2]) {
    case 'n': *c = '\n'; break;
    case 't': *c = '\t'; break;
    case 'r': *c = '\r'; break;
    case 'a': *c = '\a'; break;
    case 'b': *c = '\b'; break;
    case 'f': *c = '\f'; break;
    case 'v': *c = '\v'; break;
    case '\\': *c = '\\'; break;
    case '\'': *c = '\''; break;
    case '"': *c = '"'; break;
    case 'x': {
        char *end;
        unsigned long v = strtoul(t + 3, &end, 16);
        if (end != t + n - 1 || end == t + 3 || v > 0xff)
            return -1;
        *c = (unsigned)v;
        return 0;
    }
    default:
        if (t[2] >= '0' && t[2] <= '7') {
            char *end;
            unsigned long v = strtoul(t + 2, &end, 8);
            if (end != t + n - 1 || v > 0xff)
                return -1;
            *c = (unsigned)v;
            return 0;
        }
        return -1;
    }
    return n == 4 ? 0 : -1;               /* the quote, backslash, letter, quote */
}

static bool is_float_text(const char *t)
{
    const char *p = t;
    bool digits = false;

    if (*p == '+' || *p == '-')
        p++;
    if (!strcasecmp(p, "inf") || !strcasecmp(p, "infinity") ||
        !strcasecmp(p, "nan") || !strcasecmp(p, "nanq") || !strcasecmp(p, "nans"))
        return true;
    while (isdigit((unsigned char)*p)) {
        p++;
        digits = true;
    }
    if (*p == '.') {
        p++;
        while (isdigit((unsigned char)*p)) {
            p++;
            digits = true;
        }
    }
    if (!digits)
        return false;
    if (*p == 'e' || *p == 'E') {
        p++;
        if (*p == '+' || *p == '-')
            p++;
        if (!isdigit((unsigned char)*p))
            return false;
        while (isdigit((unsigned char)*p))
            p++;
    }
    return *p == '\0';
}

int dbxl_parse_input(const dbg_value *v, enum dbxl_style st, const char *text,
                     dbxl_input *in, char *msg, size_t msgsize)
{
    char t[256];
    int size = v->size > 0 ? v->size : 4;
    uint64_t umax = size >= 8 ? UINT64_MAX : (UINT64_C(1) << (size * 8)) - 1;

    memset(in, 0, sizeof *in);
    trim(text, t, sizeof t);
    if (st == STYLE_DEFAULT || st == STYLE_ADDRESS || st == STYLE_TYPE ||
        st == STYLE_SIZE)
        st = dbxl_default_style(v);

    switch (v->kind) {
    case DBG_K_FLOAT:
        if (st == STYLE_HEX) {
            const char *h = strncasecmp(t, "0x", 2) == 0 ? t + 2 : t;
            if (!all_digits(h, isxdigit)) {
                snprintf(msg, msgsize, "%s", HINT_HEX);
                return -1;
            }
            snprintf(in->literal, sizeof in->literal, "0x%s", h);
            in->bits = true;
            return 0;
        }
        if (!is_float_text(t)) {
            snprintf(msg, msgsize, "Invalid floating-point number: %s", t);
            return -1;
        }
        {
            const char *p = t[0] == '+' || t[0] == '-' ? t + 1 : t;
            const char *sign = t[0] == '-' ? "-" : "";
            if (!strcasecmp(p, "inf") || !strcasecmp(p, "infinity"))
                snprintf(in->literal, sizeof in->literal, "%s(1.0/0)", sign);
            else if (!strncasecmp(p, "nan", 3))
                snprintf(in->literal, sizeof in->literal, "(0.0/0)");
            else
                snprintf(in->literal, sizeof in->literal, "%s", t);
        }
        return 0;
    case DBG_K_ENUM:
    case DBG_K_OTHER:
        /* Names or numbers, as the debugger reads them. */
        snprintf(in->literal, sizeof in->literal, "%s", t);
        return 0;
    case DBG_K_BOOL:
        if (st != STYLE_HEX && st != STYLE_UNSIGNED && st != STYLE_CHARACTER) {
            size_t n = strlen(t);
            if (n && !strncasecmp(t, "true", n)) {
                snprintf(in->literal, sizeof in->literal, "1");
                return 0;
            }
            if (n && !strncasecmp(t, "false", n)) {
                snprintf(in->literal, sizeof in->literal, "0");
                return 0;
            }
            if (!all_digits(t + (t[0] == '+' || t[0] == '-'), isdigit)) {
                snprintf(msg, msgsize, "%s", HINT_LOGICAL);
                return -1;
            }
        }
        break;
    default:
        break;
    }

    /* Integers, characters, registers, pointers. */
    switch (st) {
    case STYLE_CHARACTER: {
        unsigned c;
        if (parse_char(t, &c) < 0) {
            snprintf(msg, msgsize, "%s", HINT_CHAR);
            return -1;
        }
        snprintf(in->literal, sizeof in->literal, "%u", c);
        return 0;
    }
    case STYLE_UNSIGNED: {
        const char *d = t[0] == '+' ? t + 1 : t;
        uint64_t u;
        if (!all_digits(d, isdigit)) {
            snprintf(msg, msgsize, "%s", HINT_UNSIGNED);
            return -1;
        }
        u = strtoull(d, NULL, 10);
        if (u > umax || strlen(d) > 20) {
            range_message(size, st, false, msg, msgsize);
            return -1;
        }
        snprintf(in->literal, sizeof in->literal, "%" PRIu64, u);
        return 0;
    }
    case STYLE_SIGNED: {
        const char *d = t[0] == '+' || t[0] == '-' ? t + 1 : t;
        int64_t sv, smax = (int64_t)(umax >> 1);
        if (!all_digits(d, isdigit)) {
            snprintf(msg, msgsize, "%s", HINT_SIGNED);
            return -1;
        }
        sv = strtoll(t, NULL, 10);
        if (strlen(d) > 19 || sv > smax || sv < -smax - 1) {
            range_message(size, st, true, msg, msgsize);
            return -1;
        }
        snprintf(in->literal, sizeof in->literal, "%" PRId64, sv);
        return 0;
    }
    default: {                        /* hex, pointers */
        const char *h = strncasecmp(t, "0x", 2) == 0 ? t + 2 : t;
        uint64_t u;
        if (!all_digits(h, isxdigit)) {
            snprintf(msg, msgsize, "%s", HINT_HEX);
            return -1;
        }
        u = strtoull(h, NULL, 16);
        if (strlen(h) > 16 || u > umax) {
            range_message(size, STYLE_HEX, false, msg, msgsize);
            return -1;
        }
        snprintf(in->literal, sizeof in->literal, "0x%" PRIx64, u);
        return 0;
    }
    }
}
