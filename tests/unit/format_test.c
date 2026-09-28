/*
 * Unit tests for value formatting against xldb's output (recon pass 13).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/value.h"

static int failures;

static void expect(const char *what, const char *got, const char *want)
{
    if (strcmp(got, want) != 0) {
        fprintf(stderr, "%s: got \"%s\", want \"%s\"\n", what, got, want);
        failures++;
    }
}

static void flt(double d, int digits, bool decimal, const char *want)
{
    char buf[64], what[64];

    dbxl_format_float(d, digits, decimal, buf, sizeof buf);
    snprintf(what, sizeof what, "%s %.17g", decimal ? "decimal" : "scientific", d);
    expect(what, buf, want);
}

static void scalar(enum dbg_kind kind, int size, uint64_t bits,
                   enum dbxl_style style, const char *want)
{
    static int n;
    char key[32], buf[64];
    dbg_value v;

    memset(&v, 0, sizeof v);
    v.kind = kind;
    v.size = size;
    v.bits = bits;
    v.has_addr = true;
    v.addr = 0x2ff22bec;
    v.type_style = "int";
    v.value = "0";
    snprintf(key, sizeof key, "T:x%d", n++);
    dbxl_vstate_get(key, true)->style = style;
    dbxl_format_value(&v, key, 4, buf, sizeof buf);
    expect(key, buf, want);
}

int main(void)
{
    /* float, scientific (the default) */
    flt(0.25f, 6, false, "2.5e-1");
    flt(0.1f, 6, false, "1.0e-1");
    flt(-2.5f, 6, false, "-2.5e+0");
    flt(1234567.0f, 6, false, "1.23457e+6");
    flt(1e10f, 6, false, "1.0e+10");
    flt(1e-10f, 6, false, "1.0e-10");
    flt(0.0, 6, false, "0.0");
    flt(3.14159265f, 6, false, "3.14159e+0");
    flt(1e38f, 6, false, "1.0e+38");
    flt(7.0f, 6, false, "7.0e+0");
    flt(-0.001f, 6, false, "-1.0e-3");
    flt(123.456f, 6, false, "1.23456e+2");
    /* float, decimal */
    flt(0.25f, 6, true, "0.25");
    flt(0.1f, 6, true, "0.1");
    flt(1234567.0f, 6, true, "1234570.0");
    flt(1e10f, 6, true, "1.0e+10");
    flt(0.0, 6, true, "0.0");
    flt(3.14159265f, 6, true, "3.14159");
    flt(100.0f, 6, true, "100.0");
    flt(0.3333333f, 6, true, "0.333333");
    flt(-0.001f, 6, true, "-0.001");
    flt(1e6f, 6, true, "1000000.0");
    flt(999999.0f, 6, true, "999999.0");
    flt(0.0001f, 6, true, "0.0001");
    flt(1e-5f, 6, true, "0.00001");
    flt(1e7f, 6, true, "1.0e+7");
    flt(1e-6f, 6, true, "1.0e-6");
    flt(1.25e-7f, 6, true, "1.25e-7");
    flt(12345678.0f, 6, true, "1.23457e+7");
    /* double */
    flt(3.141592653589793, 15, false, "3.14159265358979e+0");
    flt(1e100, 15, false, "1.0e+100");
    flt(123456789012.0, 15, false, "1.23456789012e+11");
    flt(1e-300, 15, false, "1.0e-300");
    flt(0.6666666666666666, 15, false, "6.66666666666667e-1");
    flt(1.5, 15, false, "1.5e+0");

    /* integers and chars */
    scalar(DBG_K_INT, 4, 100, STYLE_DEFAULT, "+100");
    scalar(DBG_K_INT, 4, 100, STYLE_CHARACTER, "'d'");
    scalar(DBG_K_INT, 4, 100, STYLE_UNSIGNED, "100");
    scalar(DBG_K_INT, 4, 100, STYLE_HEX, "0x00000064");
    scalar(DBG_K_INT, 4, 0xfffffffbu, STYLE_DEFAULT, "-5");
    scalar(DBG_K_INT, 4, 0xfffffffbu, STYLE_HEX, "0xfffffffb");
    scalar(DBG_K_INT, 4, 100, STYLE_ADDRESS, "@2ff22bec");
    scalar(DBG_K_INT, 4, 100, STYLE_SIZE, "<4>");
    scalar(DBG_K_INT, 4, 100, STYLE_TYPE, "int");
    scalar(DBG_K_CHAR, 1, 'b', STYLE_DEFAULT, "'b'");
    scalar(DBG_K_CHAR, 1, 0, STYLE_DEFAULT, "'\\0'");
    scalar(DBG_K_CHAR, 1, 'b', STYLE_SIGNED, "+98");
    scalar(DBG_K_CHAR, 1, 'b', STYLE_HEX, "0x62");
    scalar(DBG_K_UCHAR, 1, 0x81, STYLE_DEFAULT, "129");
    scalar(DBG_K_UCHAR, 1, 0x81, STYLE_HEX, "0x81");
    scalar(DBG_K_INT, 2, 0xfffd, STYLE_DEFAULT, "-3");
    scalar(DBG_K_INT, 2, 0xfffd, STYLE_HEX, "0xfffd");
    scalar(DBG_K_INT, 4, 123456789, STYLE_HEX, "0x075bcd15");

    if (failures) {
        fprintf(stderr, "format: %d failures\n", failures);
        return 1;
    }
    printf("format: ok\n");
    return 0;
}
