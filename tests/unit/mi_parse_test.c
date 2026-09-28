/*
 * Unit tests for the GDB/MI parser (make test).
 */
#include <stdio.h>
#include <string.h>

#include "backend/gdbmi/mi.h"

static int failures;

#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            fprintf(stderr, "%s:%d: CHECK(%s) failed\n", __FILE__,      \
                    __LINE__, #cond);                                   \
            failures++;                                                 \
        }                                                               \
    } while (0)

static int streq(const char *a, const char *b)
{
    return a && b && strcmp(a, b) == 0;
}

static void test_breakpoint(void)
{
    mi_record *r = mi_parse(
        "7^done,bkpt={number=\"2\",type=\"breakpoint\",disp=\"keep\","
        "enabled=\"y\",addr=\"0x0000000000001149\",func=\"main\","
        "file=\"test.c\",fullname=\"/x/test.c\",line=\"17\","
        "thread-groups=[\"i1\"],times=\"0\"}");

    CHECK(r && r->type == MI_RESULT && r->token == 7);
    CHECK(streq(r->klass, "done"));
    CHECK(streq(mi_str(r->results, "bkpt.file"), "test.c"));
    CHECK(mi_int(r->results, "bkpt.line", 0) == 17);
    CHECK(mi_int(r->results, "bkpt.addr", 0) == 0x1149);
    CHECK(mi_get(r->results, "bkpt.thread-groups")->kind == MI_LIST);
    CHECK(mi_str(r->results, "bkpt.missing") == NULL);
    mi_record_free(r);
}

static void test_stack(void)
{
    mi_record *r = mi_parse(
        "12^done,stack=[frame={level=\"0\",func=\"add\",line=\"7\"},"
        "frame={level=\"1\",func=\"main\",line=\"17\"}]");
    const mi_value *s = mi_get(r->results, "stack");
    int n = 0;

    CHECK(s && s->kind == MI_LIST);
    for (const mi_value *f = s ? s->child : NULL; f; f = f->next, n++)
        CHECK(streq(f->name, "frame") && f->kind == MI_TUPLE);
    CHECK(n == 2);
    CHECK(streq(mi_str(s->child->next, "func"), "main"));
    mi_record_free(r);
}

static void test_async_and_streams(void)
{
    mi_record *r = mi_parse("*stopped,reason=\"exited\",exit-code=\"012\"");

    CHECK(r->type == MI_EXEC && r->token == -1);
    CHECK(streq(r->klass, "stopped"));
    CHECK(streq(mi_str(r->results, "exit-code"), "012"));
    mi_record_free(r);

    r = mi_parse("*running,thread-id=\"all\"");
    CHECK(r->type == MI_EXEC && streq(r->klass, "running"));
    mi_record_free(r);

    r = mi_parse("~\"a\\tb\\n\\\"q\\\"\\\\\\101\"");
    CHECK(r->type == MI_CONSOLE && streq(r->text, "a\tb\n\"q\"\\A"));
    mi_record_free(r);

    r = mi_parse("(gdb) ");
    CHECK(r->type == MI_PROMPT);
    mi_record_free(r);

    r = mi_parse("5^error,msg=\"No symbol \\\"x\\\" in current context.\"");
    CHECK(streq(r->klass, "error"));
    CHECK(streq(mi_str(r->results, "msg"), "No symbol \"x\" in current context."));
    mi_record_free(r);

    r = mi_parse("=thread-group-added,id=\"i1\"");
    CHECK(r->type == MI_NOTIFY);
    mi_record_free(r);
}

static void test_malformed(void)
{
    CHECK(mi_parse("^done,bkpt={number=\"2\"") == NULL);
    CHECK(mi_parse("^done,x=") == NULL);
    mi_record_free(mi_parse("some program output"));
}

static void test_quote(void)
{
    char buf[64];

    mi_quote("a \"b\" \\c", buf, sizeof buf);
    CHECK(streq(buf, "\"a \\\"b\\\" \\\\c\""));
}

int main(void)
{
    test_breakpoint();
    test_stack();
    test_async_and_streams();
    test_malformed();
    test_quote();
    if (failures) {
        fprintf(stderr, "mi_parse: %d failures\n", failures);
        return 1;
    }
    printf("mi_parse: ok\n");
    return 0;
}
