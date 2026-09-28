/*
 * The Help window (DESIGN.md 10, recon passes 6 and 15).
 *
 * dbxl's own help text (help/dbxl.help, built into the program) is one
 * long document shown from its index.  Clicking a `-->` line scrolls so
 * that the named section's heading is the top line; the title bar's
 * [exit], [index] and [return] close the window, show the index and go
 * back.  The index's last link copies the text to $HOME/dbxl.help.text.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/layout.h"
#include "dbxl_help.h"                 /* generated from help/dbxl.help */
#include "help.h"
#include "ui.h"

#define TITLE "Help [exit][index][return]"
#define MAX_HISTORY 64

static struct {
    char **lines;
    int nlines;
    int history[MAX_HISTORY];
    int nhistory;
} hp;

static void split(void)
{
    int n = 0;

    if (hp.lines)
        return;
    for (int i = 0; dbxl_help[i]; i++)
        n++;
    hp.lines = calloc((size_t)n + 1, sizeof *hp.lines);
    for (int i = 0; dbxl_help[i]; i++) {
        size_t len = strcspn(dbxl_help[i], "\n");
        hp.lines[hp.nlines++] = strndup(dbxl_help[i], len);
    }
}

void dbxl_help_print(FILE *f)
{
    for (int i = 0; dbxl_help[i]; i++)
        fputs(dbxl_help[i], f);
}

void dbxl_help_open(void)
{
    xtk_pane *p = dbxl_ui_pane(DBXL_W_HELP);

    split();
    if (xtk_pane_nlines(p) == 0) {
        xtk_pane_set_lines(p, (const char *const *)hp.lines, hp.nlines);
        xtk_pane_set_title(p, TITLE);
        xtk_pane_set_top(p, 0);
    }
    dbxl_ui_show_window(DBXL_W_HELP);
}

/* "3.1  Moving within windows" and "Moving within windows" both match the
 * heading "3.1 Moving within windows": drop a leading section number,
 * squeeze spaces, ignore case. */
static bool section_number(const char *tok, size_t n)
{
    /* "3.1", "10." or "A." */
    if (n == 2 && isupper((unsigned char)tok[0]) && tok[1] == '.')
        return true;
    for (size_t i = 0; i < n; i++)
        if (!isdigit((unsigned char)tok[i]) && tok[i] != '.')
            return false;
    return n > 0;
}

static void normalize(const char *s, char *out, size_t size)
{
    size_t n = 0, tok;
    bool space = false;

    while (*s == ' ')
        s++;
    tok = strcspn(s, " ");
    if (section_number(s, tok))
        s += tok;
    for (; *s && n + 1 < size; s++) {
        if (*s == ' ') {
            space = n > 0;
            continue;
        }
        if (space)
            out[n++] = ' ';
        space = false;
        out[n++] = (char)tolower((unsigned char)*s);
    }
    out[n] = '\0';
}

/* The line of the heading a link names (a heading is underlined). */
static int find_heading(const char *link)
{
    char want[256], have[256];

    normalize(link, want, sizeof want);
    for (int i = 0; i + 1 < hp.nlines; i++) {
        if (hp.lines[i + 1][0] != '-' || hp.lines[i][0] == ' ')
            continue;
        normalize(hp.lines[i], have, sizeof have);
        if (strcmp(want, have) == 0)
            return i;
    }
    return -1;
}

static void jump(int line)
{
    xtk_pane *p = dbxl_ui_pane(DBXL_W_HELP);

    if (hp.nhistory == MAX_HISTORY) {
        memmove(hp.history, hp.history + 1, (MAX_HISTORY - 1) * sizeof hp.history[0]);
        hp.nhistory--;
    }
    hp.history[hp.nhistory++] = xtk_pane_top(p);
    xtk_pane_set_top(p, line);
}

static void copy_text(void)
{
    const char *home = getenv("HOME");
    char path[1024], msg[1200];
    FILE *f;

    snprintf(path, sizeof path, "%s/dbxl.help.text", home ? home : ".");
    f = fopen(path, "w");
    if (!f) {
        snprintf(msg, sizeof msg, "Unable to open %s", path);
        dbxl_ui_message(msg);
        return;
    }
    dbxl_help_print(f);
    fclose(f);
    snprintf(msg, sizeof msg, "The help text was written to file %s.", path);
    dbxl_ui_message(msg);
}

void dbxl_help_click(int line)
{
    const char *text, *link;
    int target;

    if (line < 0 || line >= hp.nlines)
        return;
    /* Only lines starting with "-->" are links. */
    text = hp.lines[line];
    link = text + strspn(text, " ");
    if (strncmp(link, "-->", 3) != 0)
        return;
    link += 3;
    while (*link == ' ')
        link++;
    if (strncmp(link, "copy help text", 14) == 0) {
        copy_text();
        return;
    }
    target = find_heading(link);
    if (target >= 0)
        jump(target);
}

/*
 * The title's hotspots, by column within the centred title:
 * "Help [exit][index][return]".  Returns true when one was hit.
 */
bool dbxl_help_title_click(int x, int pane_w)
{
    xtk_pane *p = dbxl_ui_pane(DBXL_W_HELP);
    int tx = (pane_w - (int)strlen(TITLE) * 8) / 2;
    int c = (x - tx) / 8;

    if (x < tx)
        return false;
    if (c >= 5 && c <= 10) {                 /* [exit] */
        xtk_pane_unmap(p);
        return true;
    }
    if (c >= 11 && c <= 17) {                /* [index] */
        jump(0);
        return true;
    }
    if (c >= 18 && c <= 25) {                /* [return] */
        if (hp.nhistory > 0)
            xtk_pane_set_top(p, hp.history[--hp.nhistory]);
        return true;
    }
    return false;
}
