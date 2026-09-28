/*
 * The Help window: dbxl's own help text (DESIGN.md 10).
 */
#ifndef DBXL_HELP_H
#define DBXL_HELP_H

#include <stdbool.h>
#include <stdio.h>

void dbxl_help_open(void);
/* A left click on a Help line: follow a `-->` link. */
void dbxl_help_click(int line);
/* A left click on the title at x: [exit], [index], [return]. */
bool dbxl_help_title_click(int x, int pane_w);
/* The whole text (-h). */
void dbxl_help_print(FILE *f);

#endif
