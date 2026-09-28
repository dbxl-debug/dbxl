/*
 * Unit tests for typed values read by display style (recon pass 15).
 */
#include <stdio.h>
#include <string.h>

#include "core/input.h"

static int failures;

static void check(enum dbg_kind kind, int size, enum dbxl_style st,
                  const char *text, const char *want_literal, const char *want_msg)
{
    dbg_value v;
    dbxl_input in;
    char msg[256] = "";
    int r;

    memset(&v, 0, sizeof v);
    v.kind = kind;
    v.size = size;
    r = dbxl_parse_input(&v, st, text, &in, msg, sizeof msg);
    if (want_literal && (r != 0 || strcmp(in.literal, want_literal) != 0)) {
        fprintf(stderr, "%s: got %s \"%s\", want \"%s\"\n", text,
                r ? "error" : "literal", r ? msg : in.literal, want_literal);
        failures++;
    }
    if (want_msg && (r == 0 || strcmp(msg, want_msg) != 0)) {
        fprintf(stderr, "%s: got %s \"%s\", want message \"%s\"\n", text,
                r ? "message" : "literal", r ? msg : in.literal, want_msg);
        failures++;
    }
}

int main(void)
{
    check(DBG_K_INT, 4, STYLE_DEFAULT, "-5", "-5", NULL);
    check(DBG_K_INT, 4, STYLE_SIGNED, "+123", "123", NULL);
    check(DBG_K_INT, 4, STYLE_SIGNED, "12x", NULL,
          "Enter a signed decimal number (examples: +123, -65091, 12).");
    check(DBG_K_INT, 2, STYLE_SIGNED, "40000", NULL,
          "Enter a value in -32768 .. 32767.");
    check(DBG_K_INT, 4, STYLE_HEX, "-5", NULL,
          "Enter an unsigned hex number (examples: 0x12,  5a, 13, deadbeef, 0x5465).");
    check(DBG_K_INT, 4, STYLE_HEX, "0xfffffffb", "0xfffffffb", NULL);
    check(DBG_K_INT, 4, STYLE_HEX, "DeadBeef", "0xdeadbeef", NULL);
    check(DBG_K_INT, 1, STYLE_HEX, "1ff", NULL, "Enter a value in 0x00 .. 0xff.");
    check(DBG_K_UINT, 4, STYLE_UNSIGNED, "+12", "12", NULL);
    check(DBG_K_UINT, 4, STYLE_UNSIGNED, "-12", NULL,
          "Enter an unsigned decimal number (examples: 123, 65091, 12).");
    check(DBG_K_UCHAR, 1, STYLE_UNSIGNED, "256", NULL, "Enter a value in 0 .. 255.");
    check(DBG_K_CHAR, 1, STYLE_DEFAULT, "'b'", "98", NULL);
    check(DBG_K_CHAR, 1, STYLE_CHARACTER, "'\\n'", "10", NULL);
    check(DBG_K_CHAR, 1, STYLE_CHARACTER, "'\\x41'", "65", NULL);
    check(DBG_K_CHAR, 1, STYLE_CHARACTER, "b", NULL,
          "Enter a quoted character (examples: 'a', ' ', '\"', ''', '\\n', '\\x12').");
    check(DBG_K_FLOAT, 4, STYLE_DEFAULT, "-12.4567e-17", "-12.4567e-17", NULL);
    check(DBG_K_FLOAT, 8, STYLE_DECIMAL, "1.2.3", NULL,
          "Invalid floating-point number: 1.2.3");
    check(DBG_K_FLOAT, 4, STYLE_HEX, "3e800000", "0x3e800000", NULL);
    check(DBG_K_BOOL, 1, STYLE_DEFAULT, "tr", "1", NULL);
    check(DBG_K_BOOL, 1, STYLE_DEFAULT, "f", "0", NULL);
    check(DBG_K_REGISTER, 8, STYLE_DEFAULT, "0x10", "0x10", NULL);
    if (failures) {
        fprintf(stderr, "input: %d failures\n", failures);
        return 1;
    }
    printf("input: ok\n");
    return 0;
}
