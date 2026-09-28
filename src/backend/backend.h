/*
 * The debugger backend interface (DESIGN.md 7).
 *
 * A backend drives one debugger engine (GDB/MI now, LLDB later) for one
 * program.  Every request returns at once; results arrive later as events
 * through the callback given at creation.  Nothing here knows about X, and
 * nothing above this interface knows about MI.
 *
 * The backend owns one descriptor that the event loop watches; when it is
 * readable the owner calls dbg_readable(), which parses what arrived and
 * delivers the events.
 */
#ifndef DBXL_BACKEND_H
#define DBXL_BACKEND_H

#include <stdbool.h>
#include <stdint.h>

typedef struct dbg_backend dbg_backend;

typedef struct dbg_launch {
    const char *program;
    int argc;                      /* program arguments */
    char **argv;
    const char *run_to;            /* function to stop in first ("main") */
    const char *tty;               /* the program's terminal; NULL = none */
} dbg_launch;

typedef struct dbg_frame {
    int level;
    uint64_t addr;
    char func[256];                /* "" when unknown */
    char file[512];                /* as compiled; "" without line info */
    char fullname[1024];           /* resolved by the engine; may be "" */
    char from[512];                /* shared object, when no file */
    int line;                      /* 0 without line info */
} dbg_frame;

/* A variable's value as a typed tree (DESIGN.md 7.1). */
enum dbg_kind {
    DBG_K_INT, DBG_K_UINT, DBG_K_CHAR, DBG_K_UCHAR, DBG_K_BOOL, DBG_K_FLOAT,
    DBG_K_ENUM, DBG_K_POINTER, DBG_K_ARRAY, DBG_K_STRUCT, DBG_K_UNION,
    DBG_K_FUNC, DBG_K_OTHER, DBG_K_ERROR,
    DBG_K_REGISTER                 /* an integer register (dbxl's own) */
};

typedef struct dbg_value {
    char *name;                    /* variable, field name or index */
    enum dbg_kind kind;
    char *type_name;               /* menu title: "int", "unsigned-char", "pointer" */
    char *type_style;              /* "-> shape-struct", "[0..3] point-struct" */
    char *tag;                     /* struct/union tag, for "shape{}" */
    int size;
    bool has_addr;
    uint64_t addr;                 /* the object's address */
    char *value;                   /* int: decimal; float: repr; enum: name;
                                      pointer: hex; error: the text */
    uint64_t bits;                 /* the value's bytes, little-endian */
    char *str;                     /* pointers, char arrays: the C string; NULL if none */
    char *expr;                    /* a debugger expression for Edit */
    long lo;                       /* arrays: the low bound */
    struct dbg_value *children;    /* arrays, structs */
    int nchildren;
    struct dbg_value *pointee;     /* only for dereferenced paths */
} dbg_value;

typedef struct dbg_value_group {
    char *file;                    /* globals: "rich.c"; locals: NULL */
    char *fullname;
    dbg_value *vars;
    int nvars;
} dbg_value_group;

enum dbg_scope { DBG_SCOPE_LOCALS, DBG_SCOPE_GLOBALS };

/* values() flags */
enum { DBG_VALUES_ALL_BLOCKS = 1 };     /* locals of inactive blocks too */

typedef struct dbg_values {
    enum dbg_scope scope;
    int level;                     /* locals: the frame */
    int ptrsize;                   /* the target's pointer size */
    dbg_value_group *groups;
    int ngroups;
} dbg_values;

void dbg_values_free(dbg_values *v);

typedef struct dbg_breakpoint {
    int id;
    bool enabled;
    char func[256];
    char file[512];
    char fullname[1024];
    int line;
    uint64_t addr;
    int nlocations;
} dbg_breakpoint;

typedef struct dbg_insn {
    uint64_t addr;
    long offset;                   /* from the function's start */
    char func[128];
    char text[160];                /* "mov    %rsp,%rbp" */
} dbg_insn;

typedef struct dbg_register {
    char name[32];                 /* as requested ("rax", "xmm0:64") */
    uint64_t value;
    int size;                      /* 0 when unavailable */
} dbg_register;

typedef struct dbg_symbol {        /* a function with debug info */
    char name[256];
    char file[256];
    char fullname[1024];
    int line;                      /* of its first instruction */
    uint64_t addr;
} dbg_symbol;

typedef struct dbg_thread {
    int id;
    long lwp;                      /* the kernel's id, 0 if unknown */
    bool current;
    char state[32];
    char func[128];
} dbg_thread;

enum dbg_stop_reason {
    DBG_STOP_BREAKPOINT,           /* a user breakpoint */
    DBG_STOP_RUN_TO,               /* the start-up (run to main) stop */
    DBG_STOP_STEP,                 /* Next/Step/Machine step/Return done */
    DBG_STOP_SIGNAL,               /* the program received a signal */
    DBG_STOP_OTHER,
};

enum dbg_event_type {
    DBG_EV_RUNNING,
    DBG_EV_STOPPED,                /* reason, frame, signal */
    DBG_EV_EXITED,                 /* exit_code */
    DBG_EV_SIGNALLED,              /* terminated by signal */
    DBG_EV_FRAMES,                 /* frames, nframes */
    DBG_EV_BP_SET,                 /* bp, cookie */
    DBG_EV_BP_DELETED,             /* bp.id, cookie */
    DBG_EV_VALUES,                 /* values (the receiver owns them), cookie */
    DBG_EV_ASSIGNED,               /* cookie */
    DBG_EV_DISASM,                 /* insns, ninsns */
    DBG_EV_REGISTERS,              /* regs, nregs */
    DBG_EV_SYMBOLS,                /* files (symbols with only file names),
                                      funcs */
    DBG_EV_DATA_START,             /* addr */
    DBG_EV_MEMORY,                 /* addr, bytes, nbytes */
    DBG_EV_THREADS,                /* threads, nthreads */
    DBG_EV_TYPES,                  /* names, nnames */
    DBG_EV_ERROR,                  /* message, request, cookie */
    DBG_EV_DIED,                   /* the engine went away */
};

/* What an error (or result) answers. */
enum dbg_request {
    DBG_REQ_OTHER,
    DBG_REQ_START,
    DBG_REQ_EXEC,
    DBG_REQ_BP_LINE,
    DBG_REQ_BP_FUNC,
    DBG_REQ_BP_DELETE,
    DBG_REQ_FRAMES,
    DBG_REQ_VALUES,
    DBG_REQ_ASSIGN,
    DBG_REQ_BP_ADDR,
    DBG_REQ_BP_ENABLE,
    DBG_REQ_BP_CONDITION,
    DBG_REQ_DISASM,
    DBG_REQ_REGISTERS,
    DBG_REQ_SYMBOLS,
    DBG_REQ_DATA_START,
    DBG_REQ_MEMORY,
    DBG_REQ_WRITE_MEMORY,
    DBG_REQ_THREADS,
    DBG_REQ_TYPES,
};

typedef struct dbg_event {
    enum dbg_event_type type;
    enum dbg_stop_reason reason;
    dbg_frame frame;
    int bkptno;                    /* the breakpoint hit, 0 if none */
    int signo;                     /* 0 when not a signal */
    const char *signame;           /* "SIGSEGV" */
    int exit_code;
    const dbg_frame *frames;
    int nframes;
    dbg_breakpoint bp;
    dbg_values *values;
    const dbg_insn *insns;
    int ninsns;
    const dbg_register *regs;
    int nregs;
    const dbg_symbol *files, *funcs;
    int nfiles, nfuncs;
    const dbg_thread *threads;
    int nthreads;
    const char *const *names;
    int nnames;
    uint64_t addr;
    const unsigned char *bytes;
    int nbytes;
    const char *message;
    enum dbg_request request;
    void *cookie;                  /* passed through from the request */
} dbg_event;

typedef void (*dbg_event_fn)(const dbg_event *ev, void *arg);

/* The operations every backend implements. */
struct dbg_backend_ops {
    int (*start)(dbg_backend *b, const dbg_launch *l);
    void (*shutdown)(dbg_backend *b);
    int (*fd)(dbg_backend *b);
    void (*readable)(dbg_backend *b);

    void (*cont)(dbg_backend *b);
    void (*next)(dbg_backend *b);
    void (*step)(dbg_backend *b);
    void (*step_insn)(dbg_backend *b);
    void (*finish)(dbg_backend *b);
    void (*restart)(dbg_backend *b);

    /* Breakpoints: at a source line, or at a function's first instruction. */
    void (*bp_line)(dbg_backend *b, const char *file, int line, void *cookie);
    void (*bp_func)(dbg_backend *b, const char *func, void *cookie);
    void (*bp_delete)(dbg_backend *b, int id, void *cookie);
    void (*bp_addr)(dbg_backend *b, uint64_t addr, void *cookie);
    void (*bp_enable)(dbg_backend *b, int id, bool on, void *cookie);
    /* cond NULL or "" makes it unconditional. */
    void (*bp_condition)(dbg_backend *b, int id, const char *cond, void *cookie);

    void (*frames)(dbg_backend *b, int max);

    /* The program's process id, 0 when it isn't running. */
    int (*pid)(dbg_backend *b);

    /*
     * Values: a frame's locals or all globals, as trees.  Pointers are
     * followed only at the given paths ("sp*", "sp*.corners*": a variable
     * name, then ".field", "[i]" or "*").  Entries "range:PATH:LO:HI" show
     * the pointer at PATH as its elements LO..HI (its children), and
     * "cast:PATH:TYPE" makes it point to TYPE.
     */
    void (*values)(dbg_backend *b, enum dbg_scope scope, int level,
                   const char *const *deref, int nderef, unsigned flags,
                   void *cookie);
    /* expr = text, in the program's language. */
    void (*assign)(dbg_backend *b, const char *expr, const char *text,
                   void *cookie);

    /* The function containing addr. */
    void (*disassemble)(dbg_backend *b, uint64_t addr, void *cookie);
    /* Registers by debugger name ("rax"; "xmm0:64" is a low lane). */
    void (*registers)(dbg_backend *b, int level, const char *const *names,
                      int n, void *cookie);
    /* Files and functions; all: functions without debug information too
     * (then in address order, else by name). */
    void (*symbols)(dbg_backend *b, bool all, void *cookie);
    void (*data_start)(dbg_backend *b, void *cookie);
    void (*read_memory)(dbg_backend *b, uint64_t addr, int len, void *cookie);
    void (*write_memory)(dbg_backend *b, uint64_t addr,
                         const unsigned char *bytes, int len, void *cookie);
    void (*threads)(dbg_backend *b, void *cookie);
    /* The program's own type names (for Cast), in declaration order. */
    void (*types)(dbg_backend *b, void *cookie);
};

struct dbg_backend {
    const struct dbg_backend_ops *ops;
    dbg_event_fn fn;
    void *arg;
};

/* Engines. */
dbg_backend *dbg_gdbmi_create(dbg_event_fn fn, void *arg);

/* Convenience wrappers. */
static inline int dbg_start(dbg_backend *b, const dbg_launch *l) { return b->ops->start(b, l); }
static inline void dbg_shutdown(dbg_backend *b) { b->ops->shutdown(b); }
static inline int dbg_fd(dbg_backend *b) { return b->ops->fd(b); }
static inline void dbg_readable(dbg_backend *b) { b->ops->readable(b); }

/* Signal number -> xldb's text, e.g. "11: SIGSEGV (segmentation violation)". */
void dbg_signal_text(int signo, const char *name, char *buf, int size);

#endif
