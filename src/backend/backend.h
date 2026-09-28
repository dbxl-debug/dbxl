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

typedef struct dbg_breakpoint {
    int id;
    bool enabled;
    char file[512];
    char fullname[1024];
    int line;
    int nlocations;
} dbg_breakpoint;

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
};

typedef struct dbg_event {
    enum dbg_event_type type;
    enum dbg_stop_reason reason;
    dbg_frame frame;
    int signo;                     /* 0 when not a signal */
    const char *signame;           /* "SIGSEGV" */
    int exit_code;
    const dbg_frame *frames;
    int nframes;
    dbg_breakpoint bp;
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

    void (*frames)(dbg_backend *b, int max);

    /* The program's process id, 0 when it isn't running. */
    int (*pid)(dbg_backend *b);
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
