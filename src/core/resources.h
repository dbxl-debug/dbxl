/*
 * X resources (DESIGN.md 3).  Resources are looked up as NAME.resource,
 * class Dbxl.Resource, where NAME is -name, else $RESOURCE_NAME, else the
 * basename of argv[0] ("dbxl").  Sources, later ones winning: the server's
 * RESOURCE_MANAGER property (xrdb), then $HOME/.Xdefaults, which is what
 * xldb itself read.
 */
#ifndef DBXL_RESOURCES_H
#define DBXL_RESOURCES_H

#include <stdbool.h>
#include <X11/Xlib.h>

void dbxl_res_init(Display *dpy, const char *name);
void dbxl_res_free(void);
const char *dbxl_res_name(void);

/* NULL / default when the resource is not set. */
const char *dbxl_res_str(const char *resource);
bool dbxl_res_bool(const char *resource, bool dflt);
int dbxl_res_int(const char *resource, int dflt);

/* The resource name to use when none is given on the command line. */
const char *dbxl_res_default_name(const char *argv0);

#endif
