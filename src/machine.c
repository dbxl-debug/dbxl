/*
 * Disassembly, Registers, Storage and Threads (recon passes 5, 10, 14).
 *
 * Disassembly lists the function around the pc, titled with its name,
 * one `0x<addr> (+0x<off>)  <mnemonic> <operands>` row per instruction
 * (the first offset written `(+000000)`), with the arrow, stop signs and
 * cyan selection of Source, and is centred on the pc like Source.  A click
 * sets or clears a breakpoint at the instruction.
 *
 * Registers shows dbxl's own register table (DESIGN.md 6.1) in xldb's
 * `%-5s: 0x...` rows; a click opens the variable menu titled intregister.
 *
 * Storage (`Storage Pane`) is a hex dump starting at the program's data:
 * `<ADDR>:  <word> <word> <word> <word>  <16 characters>`.  A click on a
 * word opens the `Storage` cell menu at the word; Edit is an in-place
 * field over it.
 */
#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/layout.h"
#include "core/options.h"
#include "core/value.h"
#include "data.h"
#include "debugger.h"
#include "machine.h"
#include "ui.h"

#define STORAGE_ROWS 256              /* 4 KiB fetched at a time */

/* ---- the register table (x86-64, DESIGN.md 6.1) -------------------------- */

enum group { G_SPECIAL, G_GENERAL, G_DOUBLE, G_FLOAT };

struct reg {
    const char *label;                /* shown, upper case */
    const char *gdb;                  /* requested from the backend */
    const char *expr;                 /* for Edit and triggers */
    int bytes;                        /* display width / 2 */
    enum group group;
};

static const struct reg x86_64_regs[] = {
    { "RIP", "rip", "$rip", 8, G_SPECIAL },
    { "EFLAGS", "eflags", "$eflags", 4, G_SPECIAL },
    { "CS", "cs", "$cs", 2, G_SPECIAL },
    { "SS", "ss", "$ss", 2, G_SPECIAL },
    { "DS", "ds", "$ds", 2, G_SPECIAL },
    { "ES", "es", "$es", 2, G_SPECIAL },
    { "FS", "fs", "$fs", 2, G_SPECIAL },
    { "GS", "gs", "$gs", 2, G_SPECIAL },
    { "FS_BASE", "fs_base", "$fs_base", 8, G_SPECIAL },
    { "GS_BASE", "gs_base", "$gs_base", 8, G_SPECIAL },
    { "MXCSR", "mxcsr", "$mxcsr", 4, G_SPECIAL },
    { "RAX", "rax", "$rax", 8, G_GENERAL },
    { "RBX", "rbx", "$rbx", 8, G_GENERAL },
    { "RCX", "rcx", "$rcx", 8, G_GENERAL },
    { "RDX", "rdx", "$rdx", 8, G_GENERAL },
    { "RSI", "rsi", "$rsi", 8, G_GENERAL },
    { "RDI", "rdi", "$rdi", 8, G_GENERAL },
    { "RBP", "rbp", "$rbp", 8, G_GENERAL },
    { "RSP", "rsp", "$rsp", 8, G_GENERAL },
    { "R8", "r8", "$r8", 8, G_GENERAL },
    { "R9", "r9", "$r9", 8, G_GENERAL },
    { "R10", "r10", "$r10", 8, G_GENERAL },
    { "R11", "r11", "$r11", 8, G_GENERAL },
    { "R12", "r12", "$r12", 8, G_GENERAL },
    { "R13", "r13", "$r13", 8, G_GENERAL },
    { "R14", "r14", "$r14", 8, G_GENERAL },
    { "R15", "r15", "$r15", 8, G_GENERAL },
#define XMM(n) \
    { "XMM" #n, "xmm" #n ":64", "$xmm" #n ".v2_int64[0]", 8, G_DOUBLE }, \
    { "XMM" #n, "xmm" #n ":32", "$xmm" #n ".v4_int32[0]", 4, G_FLOAT }
    XMM(0), XMM(1), XMM(2), XMM(3), XMM(4), XMM(5), XMM(6), XMM(7),
    XMM(8), XMM(9), XMM(10), XMM(11), XMM(12), XMM(13), XMM(14), XMM(15),
#undef XMM
};
#define NREGS (int)(sizeof x86_64_regs / sizeof x86_64_regs[0])

static struct {
    dbg_backend *b;
    int ptrsize;

    /* Disassembly */
    dbg_insn *insns;
    int ninsns;
    uint64_t pc;                      /* the stop's pc (arrow) */
    uint64_t want;                    /* the instruction to centre on */
    uint64_t cyan;                    /* the selected instruction, 0 = none */
    bool cyan_before;                 /* cyan the instruction before `cyan` */
    bool have_pc;

    /* Registers */
    int level;
    dbg_value regv[NREGS];
    char regtext[NREGS][32];
    bool have_regs;
    dbxl_render rr;

    /* Storage */
    uint64_t base;                    /* address of the first row */
    bool have_base;
    unsigned char *mem;
    int nmem;
    uint64_t edit_addr;

    /* Threads */
    enum dbg_stop_reason reason;
    char signame[32];
} mc;

static xtk_pane *pane(int w) { return dbxl_ui_pane(w); }

/* ---- Disassembly ---------------------------------------------------------- */

static int insn_row(uint64_t addr)
{
    for (int i = 0; i < mc.ninsns; i++)
        if (mc.insns[i].addr == addr)
            return i;
    return -1;
}

/* The row of the instruction before the one at addr (a call, for a return
 * address). */
static int insn_row_before(uint64_t addr)
{
    int best = -1;

    for (int i = 0; i < mc.ninsns; i++)
        if (mc.insns[i].addr < addr)
            best = i;
    return best;
}

static void render_disasm(void)
{
    xtk_pane *p = pane(DBXL_W_DISASSEMBLY);
    char (*text)[256] = calloc((size_t)(mc.ninsns ? mc.ninsns : 1), sizeof *text);
    const char **lines = calloc((size_t)(mc.ninsns ? mc.ninsns : 1), sizeof *lines);
    unsigned char *marks = calloc((size_t)(mc.ninsns ? mc.ninsns : 1), 1);
    int w = mc.ptrsize * 2, arrow = -1, sel = -1, centre;

    for (int i = 0; i < mc.ninsns; i++) {
        const dbg_insn *in = &mc.insns[i];
        char mnem[64], off[16];
        const char *ops = in->text;
        size_t n = strcspn(ops, " \t");

        snprintf(mnem, sizeof mnem, "%.*s", (int)(n < 63 ? n : 63), ops);
        ops += n;
        while (*ops == ' ' || *ops == '\t')
            ops++;
        if (in->offset == 0)
            snprintf(off, sizeof off, "(+000000)");
        else
            snprintf(off, sizeof off, "(+0x%04lx)", in->offset);
        snprintf(text[i], sizeof text[i], "0x%0*" PRIx64 " %s  %-8s %s", w,
                 in->addr, off, mnem, ops);
        lines[i] = text[i];
        marks[i] = dbxl_debug_addr_marks(in->addr);
        if (mc.have_pc && in->addr == mc.pc) {
            marks[i] |= XTK_MARK_ARROW;
            arrow = i;
        }
    }
    if (mc.cyan)
        sel = mc.cyan_before ? insn_row_before(mc.cyan) : insn_row(mc.cyan);
    xtk_pane_set_title(p, mc.ninsns ? mc.insns[0].func : "Disassembly");
    xtk_pane_set_lines(p, lines, mc.ninsns);
    xtk_pane_set_marks(p, marks, mc.ninsns);
    xtk_pane_set_selected(p, sel);
    centre = sel >= 0 ? sel : insn_row(mc.want) >= 0 ? insn_row(mc.want) : arrow;
    if (centre >= 0) {
        int top = centre - xtk_pane_rows(p) / 2;
        xtk_pane_set_top(p, top > 0 ? top : 0);
    }
    free(marks);
    free(lines);
    free(text);
}

static void show_addr(uint64_t addr)
{
    mc.want = addr;
    if (insn_row(addr) >= 0) {        /* already listed */
        render_disasm();
        return;
    }
    if (mc.b)
        mc.b->ops->disassemble(mc.b, addr, NULL);
}

void dbxl_machine_marks_changed(void)
{
    if (mc.ninsns)
        render_disasm();
}

/* ---- Registers ------------------------------------------------------------ */

static bool group_shown(enum group g)
{
    switch (g) {
    case G_SPECIAL: return dbxl_opt.reg[REG_SPECIAL];
    case G_GENERAL: return dbxl_opt.reg[REG_GENERAL];
    case G_DOUBLE: return dbxl_opt.reg[REG_DOUBLE];
    default: return false;          /* Float: the floatRegisters resource */
    }
}

static void request_registers(void)
{
    const char *names[NREGS];

    if (!mc.b)
        return;
    for (int i = 0; i < NREGS; i++)
        names[i] = x86_64_regs[i].gdb;
    mc.b->ops->registers(mc.b, mc.level, names, NREGS, NULL);
}

static void render_registers(void)
{
    dbxl_render_free(&mc.rr);
    dbxl_render_init(&mc.rr, mc.ptrsize);
    if (mc.have_regs)
        for (int i = 0; i < NREGS; i++) {
            char label[32];

            if (!group_shown(x86_64_regs[i].group) || mc.regv[i].size == 0)
                continue;
            snprintf(label, sizeof label, "%-5s: ", x86_64_regs[i].label);
            dbxl_render_var_label(&mc.rr, "R", label, &mc.regv[i]);
        }
    xtk_pane_set_lines(pane(DBXL_W_REGISTERS),
                       (const char *const *)mc.rr.lines, mc.rr.nlines);
}

static void got_registers(const dbg_register *regs, int n)
{
    for (int i = 0; i < NREGS; i++) {
        dbg_value *v = &mc.regv[i];
        const struct reg *r = &x86_64_regs[i];
        static char tname[] = "intregister";

        memset(v, 0, sizeof *v);
        v->name = (char *)r->label;
        v->kind = DBG_K_REGISTER;
        v->type_name = tname;
        v->type_style = r->bytes == 8 ? "unsigned-long"
                      : r->bytes == 4 ? "unsigned-int" : "unsigned-short";
        v->tag = "";
        v->value = mc.regtext[i];
        v->expr = (char *)r->expr;
        for (int k = 0; k < n; k++)
            if (strcmp(regs[k].name, r->gdb) == 0 && regs[k].size > 0) {
                v->size = r->bytes;
                v->bits = regs[k].value;
                snprintf(mc.regtext[i], sizeof mc.regtext[i], "%" PRIu64,
                         regs[k].value);
            }
    }
    mc.have_regs = true;
    render_registers();
}

/* ---- Storage -------------------------------------------------------------- */

static void request_memory(void)
{
    if (mc.b && mc.have_base)
        mc.b->ops->read_memory(mc.b, mc.base, STORAGE_ROWS * 16, NULL);
}

static void render_storage(void)
{
    xtk_pane *p = pane(DBXL_W_STORAGE);
    int rows = mc.nmem / 16, w = mc.ptrsize * 2;
    char (*text)[128] = calloc((size_t)(rows ? rows : 1), sizeof *text);
    const char **lines = calloc((size_t)(rows ? rows : 1), sizeof *lines);

    for (int r = 0; r < rows; r++) {
        const unsigned char *b = mc.mem + r * 16;
        int n = snprintf(text[r], sizeof text[r], "%0*" PRIX64 ": ", w,
                         mc.base + (uint64_t)r * 16);

        for (int k = 0; k < 4; k++)
            n += snprintf(text[r] + n, sizeof text[r] - (size_t)n,
                          " %02X%02X%02X%02X", b[4 * k], b[4 * k + 1],
                          b[4 * k + 2], b[4 * k + 3]);
        n += snprintf(text[r] + n, sizeof text[r] - (size_t)n, "  ");
        /* Unprintable bytes show as xldb's 0xFA (u acute in ISO 8859-1). */
        for (int k = 0; k < 16; k++)
            text[r][n++] = (char)(b[k] >= 32 && b[k] < 127 ? b[k] : 0xfa);
        text[r][n] = '\0';
        lines[r] = text[r];
    }
    xtk_pane_set_lines(p, lines, rows);
    free(lines);
    free(text);
}

void dbxl_machine_storage_view(uint64_t addr)
{
    char msg[64];

    if (!xtk_pane_mapped(pane(DBXL_W_STORAGE))) {
        dbxl_ui_message("Storage view ignored. Storage pane is hidden");
        return;
    }
    mc.base = addr & ~(uint64_t)15;
    mc.have_base = true;
    xtk_pane_set_top(pane(DBXL_W_STORAGE), 0);
    request_memory();
    snprintf(msg, sizeof msg, "Storage target address is: %" PRIx64, addr);
    dbxl_ui_message(msg);
}

static void edit_done(const char *text, void *arg)
{
    unsigned char bytes[4];
    uint64_t v;
    char *end;

    (void)arg;
    xtk_pane_set_title(pane(DBXL_W_STORAGE), "Storage Pane");
    if (!text || !mc.b)
        return;
    v = strtoull(text, &end, 16);
    if (end == text)
        return;
    /* The word as shown: its bytes in memory order. */
    for (int i = 0; i < 4; i++)
        bytes[i] = (unsigned char)(v >> (8 * (3 - i)));
    mc.b->ops->write_memory(mc.b, mc.edit_addr, bytes, 4, NULL);
}

struct storage_click {
    uint64_t addr;
    int px, py;                       /* the word's cell, frame coordinates */
    char text[16];
};
static struct storage_click sclick;

static void storage_menu_chosen(xtk_menu *m, int item, int button, void *arg)
{
    char title[64];

    (void)button;
    (void)arg;
    xtk_menu_close(m, false);
    switch (item) {
    case 0:                           /* Edit: a field exactly over the word */
        mc.edit_addr = sclick.addr;
        snprintf(title, sizeof title, "Change Block at Address: %0*" PRIX64,
                 mc.ptrsize * 2, sclick.addr);
        xtk_pane_set_title(pane(DBXL_W_STORAGE), title);
        xtk_field_open(sclick.px - 2, sclick.py - 2, 64, 13, sclick.text, 0,
                       edit_done, NULL);
        break;
    case 2:                           /* Storage view */
        dbxl_machine_storage_view(sclick.addr);
        break;
    default:                          /* Breakpoint: a later milestone */
        break;
    }
}

static void storage_click(const xtk_pane_event *e)
{
    static const char *const items[] = { "Edit", "Breakpoint", "Storage view" };
    xtk_menu_spec spec = { "Storage", items, 3, 112, 0 };
    xtk_rect g = xtk_pane_geometry(pane(DBXL_W_STORAGE));
    int w = mc.ptrsize * 2, first = w + 3, k, row = e->line;

    if (row < 0 || row >= mc.nmem / 16 || e->col < first)
        return;
    k = (e->col - first) / 9;
    if (k > 3 || (e->col - first) % 9 == 8)
        return;
    sclick.addr = mc.base + (uint64_t)row * 16 + (uint64_t)k * 4;
    snprintf(sclick.text, sizeof sclick.text, "%.8s",
             xtk_pane_line(pane(DBXL_W_STORAGE), row) + first + 9 * k);
    /* The word's cell, inside the pane's 2px border. */
    sclick.px = g.x + 2 + (first + 9 * k) * 8;
    sclick.py = g.y + 2 + 13 + e->row * 13;
    xtk_menu_open_at(&spec, sclick.px - 2, sclick.py - 2, e->fx, e->fy,
                     storage_menu_chosen, NULL);
}

/* ---- Threads -------------------------------------------------------------- */

static void render_threads(const dbg_thread *t, int n)
{
    char (*text)[128] = calloc((size_t)n + 2, sizeof *text);
    const char **lines = calloc((size_t)n + 2, sizeof *lines);
    int sel = -1;
    char reason[32];

    snprintf(text[0], sizeof text[0], "%s",
             "-------Label-------  -Thread-  -Mode-  --Wait--  --wchan-  "
             "----SIGNAL---  --State-");
    snprintf(text[1], sizeof text[1], "    All Threads");
    if (mc.reason == DBG_STOP_SIGNAL && mc.signame[0])
        snprintf(reason, sizeof reason, "%s", mc.signame);
    else
        snprintf(reason, sizeof reason, "Breakpoint-05");
    for (int i = 0; i < n; i++) {
        /* Columns as xldb: the id ends at 28, the mode at 32, the reason at
         * 59, the state at 75 (recon pass 14). */
        snprintf(text[i + 2], sizeof text[i + 2], "%29ld   %-4s%23s%-13.13s   %s",
                 t[i].lwp ? t[i].lwp : (long)t[i].id, "user", "", reason,
                 "Enabled");
        if (t[i].current)
            sel = i + 2;
    }
    for (int i = 0; i < n + 2; i++)
        lines[i] = text[i];
    xtk_pane_set_lines(pane(DBXL_W_THREADS), lines, n + 2);
    xtk_pane_set_selected(pane(DBXL_W_THREADS), sel);
    free(lines);
    free(text);
}

/* ---- events and requests ---------------------------------------------------- */

void dbxl_machine_set_backend(dbg_backend *b)
{
    mc.b = b;
    if (b)
        b->ops->data_start(b, NULL);
}

void dbxl_machine_stopped(uint64_t pc, enum dbg_stop_reason reason,
                          const char *signame)
{
    mc.pc = pc;
    mc.have_pc = true;
    mc.cyan = 0;
    mc.cyan_before = false;
    mc.level = 0;
    mc.reason = reason;
    snprintf(mc.signame, sizeof mc.signame, "%s", signame ? signame : "");
    show_addr(pc);
    request_registers();
    request_memory();
    if (mc.b)
        mc.b->ops->threads(mc.b, NULL);
}

void dbxl_machine_select_frame(int level, uint64_t pc)
{
    mc.level = level;
    mc.cyan = pc;
    mc.cyan_before = level > 0;       /* a caller: its call instruction */
    show_addr(pc);
    request_registers();
}

void dbxl_machine_show_function(uint64_t addr)
{
    mc.cyan = addr;
    mc.cyan_before = false;
    show_addr(addr);
}

void dbxl_machine_exited(void)
{
    mc.have_pc = false;
    mc.cyan = 0;
    if (mc.ninsns)
        render_disasm();
}

void dbxl_machine_event(const dbg_event *ev)
{
    switch (ev->type) {
    case DBG_EV_DISASM:
        free(mc.insns);
        mc.insns = calloc((size_t)(ev->ninsns ? ev->ninsns : 1), sizeof *mc.insns);
        memcpy(mc.insns, ev->insns, (size_t)ev->ninsns * sizeof *mc.insns);
        mc.ninsns = ev->ninsns;
        render_disasm();
        break;
    case DBG_EV_REGISTERS:
        got_registers(ev->regs, ev->nregs);
        break;
    case DBG_EV_DATA_START:
        if (!mc.have_base && ev->addr) {
            mc.base = ev->addr & ~(uint64_t)15;
            mc.have_base = true;
            request_memory();
        }
        break;
    case DBG_EV_MEMORY:
        free(mc.mem);
        mc.mem = malloc((size_t)(ev->nbytes ? ev->nbytes : 1));
        memcpy(mc.mem, ev->bytes, (size_t)ev->nbytes);
        mc.nmem = ev->nbytes;
        mc.base = ev->addr;
        render_storage();
        break;
    case DBG_EV_THREADS:
        render_threads(ev->threads, ev->nthreads);
        break;
    case DBG_EV_ASSIGNED:
        /* A register or memory write: show the new values. */
        request_registers();
        request_memory();
        break;
    default:
        break;
    }
}

void dbxl_machine_click(int window, const xtk_pane_event *e)
{
    const dbxl_span *s;

    switch (window) {
    case DBXL_W_DISASSEMBLY:
        if (e->line >= 0 && e->line < mc.ninsns)
            dbxl_debug_toggle_addr(mc.insns[e->line].addr);
        break;
    case DBXL_W_REGISTERS:
        s = dbxl_render_hit(&mc.rr, e->line, e->col);
        if (s)
            dbxl_data_open_menu(s, e->fx, e->fy);
        break;
    case DBXL_W_STORAGE:
        storage_click(e);
        break;
    default:
        break;
    }
}

void dbxl_machine_init(void)
{
    mc.ptrsize = 8;
    dbxl_render_init(&mc.rr, 8);
}
