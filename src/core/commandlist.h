/*
 * The commandList resource (DESIGN.md 8): the Commands pane's entries.
 *
 *   commandList: CommandDefinition...
 *     +                  all default commands
 *     Name=Action        Name may be "quoted"; Action is one of the verbs
 *     Name=Action:k      below, or Subprogram() to call in the program;
 *                        :k adds the accelerator Alt-k
 */
#ifndef DBXL_COMMANDLIST_H
#define DBXL_COMMANDLIST_H

enum dbxl_action {
    ACT_NULL, ACT_CONTINUE, ACT_NEXT, ACT_STEP, ACT_MACHINE_STEP, ACT_RETURN,
    ACT_SIGNAL, ACT_EDIT, ACT_RESTART, ACT_EXIT, ACT_BREAKPOINT, ACT_OPTIONS,
    ACT_HELP, ACT_CALL
};

struct dbxl_command {
    char name[64];            /* as displayed, including padding spaces */
    enum dbxl_action action;
    char call[64];            /* ACT_CALL: subprogram name */
    char key;                 /* accelerator, 0 = none */
};

#define DBXL_MAX_COMMANDS 64

/* Parse; returns the number of commands (defaults if spec is NULL). */
int dbxl_commandlist_parse(const char *spec, struct dbxl_command *out, int max);
/* The line shown in the Commands pane. */
void dbxl_command_line(const struct dbxl_command *c, char *buf, int size);

#endif
