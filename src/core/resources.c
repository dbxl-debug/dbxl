#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <X11/Xresource.h>

#include "core/resources.h"

static XrmDatabase db;
static char res_name[128] = "dbxl";

const char *dbxl_res_default_name(const char *argv0)
{
    const char *env = getenv("RESOURCE_NAME");
    const char *base;

    if (env && *env)
        return env;
    base = argv0 ? strrchr(argv0, '/') : NULL;
    return base ? base + 1 : (argv0 && *argv0 ? argv0 : "dbxl");
}

void dbxl_res_init(Display *dpy, const char *name)
{
    const char *rm = XResourceManagerString(dpy);
    const char *home = getenv("HOME");

    XrmInitialize();
    snprintf(res_name, sizeof res_name, "%s", name ? name : "dbxl");
    if (rm)
        db = XrmGetStringDatabase(rm);
    if (home) {
        char path[4096];
        XrmDatabase file;

        snprintf(path, sizeof path, "%s/.Xdefaults", home);
        file = XrmGetFileDatabase(path);
        if (file)
            XrmMergeDatabases(file, &db);   /* file wins */
    }
}

void dbxl_res_free(void)
{
    if (db)
        XrmDestroyDatabase(db);
    db = NULL;
}

const char *dbxl_res_name(void)
{
    return res_name;
}

const char *dbxl_res_str(const char *resource)
{
    char name[256], cls[256];
    char *type;
    XrmValue v;

    if (!db)
        return NULL;
    snprintf(name, sizeof name, "%s.%s", res_name, resource);
    snprintf(cls, sizeof cls, "Dbxl.%c%s",
             toupper((unsigned char)resource[0]), resource + 1);
    if (XrmGetResource(db, name, cls, &type, &v) && v.addr)
        return v.addr;
    return NULL;
}

bool dbxl_res_bool(const char *resource, bool dflt)
{
    const char *s = dbxl_res_str(resource);

    if (!s)
        return dflt;
    while (isspace((unsigned char)*s))
        s++;
    if (!strncasecmp(s, "true", 4) || !strncasecmp(s, "yes", 3) ||
        !strncasecmp(s, "on", 2) || *s == '1')
        return true;
    if (!strncasecmp(s, "false", 5) || !strncasecmp(s, "no", 2) ||
        !strncasecmp(s, "off", 3) || *s == '0')
        return false;
    return dflt;
}

int dbxl_res_int(const char *resource, int dflt)
{
    const char *s = dbxl_res_str(resource);
    char *end;
    long v;

    if (!s)
        return dflt;
    v = strtol(s, &end, 10);
    return end == s ? dflt : (int)v;
}
