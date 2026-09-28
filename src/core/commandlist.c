#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "core/commandlist.h"

/* xldb's compiled-in default (R_default_commands, recon pass 1). */
static const char default_commands[] =
    "\"Continue         \"=continue:c \"Next             \"=next:n "
    "\"Step             \"=step:s \"Machine step     \"=machine-step:m "
    "\"Return           \"=return:r \"Signal           \"=signal:g "
    "\"Edit             \"=edit:e \"Restart          \"=restart:t "
    "\"Exit             \"=exit:x \"Breakpoint       \"=breakpoint:b "
    "\"Options          \"=options:o \"Help             \"=help:h \"\"=null";

static const struct {
    const char *verb;
    enum dbxl_action action;
} verbs[] = {
    { "continue", ACT_CONTINUE }, { "next", ACT_NEXT }, { "step", ACT_STEP },
    { "machine-step", ACT_MACHINE_STEP }, { "return", ACT_RETURN },
    { "signal", ACT_SIGNAL }, { "edit", ACT_EDIT }, { "restart", ACT_RESTART },
    { "exit", ACT_EXIT }, { "breakpoint", ACT_BREAKPOINT },
    { "options", ACT_OPTIONS }, { "help", ACT_HELP }, { "null", ACT_NULL },
};

/* Read one token (quoted or up to = or space) into buf; return the end. */
static const char *token(const char *s, char *buf, size_t size, const char *stop)
{
    size_t n = 0;

    if (*s == '"') {
        for (s++; *s && *s != '"'; s++)
            if (n + 1 < size)
                buf[n++] = *s;
        if (*s == '"')
            s++;
    } else {
        for (; *s && !isspace((unsigned char)*s) && !strchr(stop, *s); s++)
            if (n + 1 < size)
                buf[n++] = *s;
    }
    buf[n] = '\0';
    return s;
}

static int parse(const char *s, struct dbxl_command *out, int n, int max)
{
    while (*s && n < max) {
        struct dbxl_command c;
        char act[80];

        while (isspace((unsigned char)*s) || *s == '\\')
            s++;
        if (!*s)
            break;
        if (*s == '+' && (!s[1] || isspace((unsigned char)s[1]))) {
            n = parse(default_commands, out, n, max);
            s++;
            continue;
        }
        memset(&c, 0, sizeof c);
        s = token(s, c.name, sizeof c.name, "=");
        if (*s != '=') {                  /* malformed: skip the token */
            while (*s && !isspace((unsigned char)*s))
                s++;
            continue;
        }
        s = token(s + 1, act, sizeof act, ":");
        if (*s == ':') {
            c.key = s[1];
            s += s[1] ? 2 : 1;
        }
        c.action = ACT_NULL;
        for (size_t i = 0; i < sizeof verbs / sizeof verbs[0]; i++)
            if (strcmp(act, verbs[i].verb) == 0)
                c.action = verbs[i].action;
        if (c.action == ACT_NULL && strstr(act, "()")) {
            c.action = ACT_CALL;
            snprintf(c.call, sizeof c.call, "%.*s",
                     (int)(strstr(act, "()") - act), act);
        }
        out[n++] = c;
    }
    return n;
}

int dbxl_commandlist_parse(const char *spec, struct dbxl_command *out, int max)
{
    return parse(spec ? spec : default_commands, out, 0, max);
}

void dbxl_command_line(const struct dbxl_command *c, char *buf, int size)
{
    if (c->key)
        snprintf(buf, (size_t)size, "%sAlt-%c", c->name, c->key);
    else
        snprintf(buf, (size_t)size, "%s", c->name);
}
