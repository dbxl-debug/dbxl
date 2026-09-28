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

/* An int[5] { 1..5 } and a render of it at `detail`, lines joined by |. */
static void render_counts(int detail, const char *want)
{
    static int n;
    dbg_value v, ch[5];
    dbxl_render r;
    char scope[32], key[64], got[512] = "";

    memset(&v, 0, sizeof v);
    memset(ch, 0, sizeof ch);
    v.name = "counts";
    v.kind = DBG_K_ARRAY;
    v.size = 20;
    v.children = ch;
    v.nchildren = 5;
    v.value = "";
    for (int i = 0; i < 5; i++) {
        ch[i].kind = DBG_K_INT;
        ch[i].size = 4;
        ch[i].bits = (uint64_t)(i + 1);
        ch[i].value = "0";
    }
    snprintf(scope, sizeof scope, "C%d", n++);
    snprintf(key, sizeof key, "%s:counts", scope);
    dbxl_vstate_get(key, true)->detail = detail;
    dbxl_render_init(&r, 4);
    dbxl_render_var(&r, scope, &v);
    for (int i = 0; i < r.nlines; i++) {
        size_t len = strlen(r.lines[i]);
        while (len > 0 && r.lines[i][len - 1] == ' ')
            len--;                     /* trailing padding isn't drawn */
        snprintf(got + strlen(got), sizeof got - strlen(got), "%s%.*s",
                 i ? "|" : "", (int)len, r.lines[i]);
    }
    dbxl_render_free(&r);
    expect("counts", got, want);
}

/* A scalar rendered as a variable (justification applies there). */
static void render_scalar(const char *name, enum dbg_kind kind, int size,
                          uint64_t bits, const char *value, const char *want)
{
    dbg_value v;
    dbxl_render r;

    memset(&v, 0, sizeof v);
    v.name = (char *)name;
    v.kind = kind;
    v.size = size;
    v.bits = bits;
    v.value = (char *)value;
    dbxl_render_init(&r, 4);
    dbxl_render_var(&r, "S", &v);
    expect(name, r.lines[0], want);
    dbxl_render_free(&r);
}

static void string_ptr(const char *str, const char *want)
{
    static int n;
    char key[32], buf[128];
    dbg_value v;

    memset(&v, 0, sizeof v);
    v.kind = DBG_K_POINTER;
    v.size = 4;
    v.value = "20000410";
    v.str = (char *)str;
    snprintf(key, sizeof key, "T:s%d", n++);
    dbxl_vstate_get(key, true)->style = STYLE_STRING;
    dbxl_format_value(&v, key, 4, buf, sizeof buf);
    expect(key, buf, want);
}

/* Recon pass 17: resources and -e. */
static void resources(void)
{
    /* scalarJustification (fields: 11 for 4 bytes, 6 for 1 and 2, 12 for
     * float) */
    dbxl_fmt.justify = JUSTIFY_RIGHT;
    render_scalar("a", DBG_K_INT, 4, 0, "0", "a:          +0");
    render_scalar("delta", DBG_K_INT, 2, 0xfffd, "-3", "delta:     -3");
    render_scalar("flags", DBG_K_UCHAR, 1, 0x81, "129", "flags:    129");
    render_scalar("tag", DBG_K_CHAR, 1, 0, "0", "tag:   '\\0'");
    render_scalar("ratio", DBG_K_FLOAT, 4, 0, "0.25", "ratio:       2.5e-1");
    render_counts(2, "counts: [          +1          +2          +3          +4"
                     "          +5 ]");
    dbxl_fmt.justify = JUSTIFY_CENTER;
    render_scalar("delta", DBG_K_INT, 2, 0xfffd, "-3", "delta:   -3  ");
    render_scalar("ratio", DBG_K_FLOAT, 4, 0, "0.25", "ratio:    2.5e-1   ");
    render_counts(2, "counts: [     +1          +2          +3          +4"
                     "          +5      ]");
    dbxl_fmt.justify = JUSTIFY_LEFT;
    render_counts(2, "counts: [ +1          +2          +3          +4"
                     "          +5          ]");
    dbxl_fmt.justify = JUSTIFY_NONE;

    /* maxArrayElements (-e 2) */
    dbxl_fmt.max_array = 2;
    render_counts(2, "counts: [ +1 +2  ...]");
    render_counts(3, "counts: [ [ 0]: +1|          [ 1]: +2|           ...]");
    dbxl_fmt.max_array = 1000;

    /* maxString */
    dbxl_fmt.max_string = 5;
    string_ptr("hello, world", "\"hello\"...");
    string_ptr("hello", "\"hello\"");
    dbxl_fmt.max_string = 200;

    /* signed/unsignedIntegerStyles, floatStyles */
    {
        enum dbxl_style st[4] = { 0 };
        if (!dbxl_parse_styles("hex - hex", st, 4) || st[0] != STYLE_HEX ||
            st[1] != STYLE_DEFAULT || st[2] != STYLE_HEX || st[3] != STYLE_DEFAULT) {
            fprintf(stderr, "parse_styles: wrong\n");
            failures++;
        }
        if (dbxl_parse_styles("octal", st, 4)) {
            fprintf(stderr, "parse_styles: accepted octal\n");
            failures++;
        }
    }
    dbxl_parse_styles("hex - hex", dbxl_fmt.signed_style, 4);
    dbxl_parse_styles("signed", dbxl_fmt.unsigned_style, 4);
    dbxl_parse_styles("scientific decimal", dbxl_fmt.float_style, 3);
    scalar(DBG_K_INT, 4, 123456789, STYLE_DEFAULT, "0x075bcd15");
    scalar(DBG_K_INT, 2, 0xfffd, STYLE_DEFAULT, "-3");
    scalar(DBG_K_UCHAR, 1, 0x81, STYLE_DEFAULT, "-127");
    render_scalar("scale", DBG_K_FLOAT, 8, 0, "1.5", "scale: 1.5");
    render_scalar("ratio", DBG_K_FLOAT, 4, 0, "0.25", "ratio: 2.5e-1");
    dbxl_format_defaults();
    render_scalar("scale", DBG_K_FLOAT, 8, 0, "1.5", "scale: 1.5e+0");
    /* floatDigits */
    dbxl_fmt.float_digits[0] = 3;
    render_scalar("third", DBG_K_FLOAT, 4, 0, "0.333333", "third: 3.33e-1");
    dbxl_format_defaults();
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

    resources();

    if (failures) {
        fprintf(stderr, "format: %d failures\n", failures);
        return 1;
    }
    printf("format: ok\n");
    return 0;
}
