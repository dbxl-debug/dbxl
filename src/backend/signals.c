/*
 * Signal names in xldb's message style: "11: SIGSEGV (segmentation
 * violation)" (recon pass 12).  Numbers are the host's; descriptions follow
 * xldb's lower-case wording where it was observed and the traditional
 * dbx/sys_siglist wording otherwise.
 */
#include <signal.h>
#include <stdio.h>
#include <string.h>

#include "backend/backend.h"

static const struct {
    const char *name;
    int signo;
    const char *text;
} signals[] = {
    { "SIGHUP", SIGHUP, "hangup" },
    { "SIGINT", SIGINT, "interrupt" },
    { "SIGQUIT", SIGQUIT, "quit" },
    { "SIGILL", SIGILL, "illegal instruction" },
    { "SIGTRAP", SIGTRAP, "trace/BPT trap" },
    { "SIGABRT", SIGABRT, "abort" },
    { "SIGBUS", SIGBUS, "bus error" },
    { "SIGFPE", SIGFPE, "floating point exception" },
    { "SIGKILL", SIGKILL, "kill" },
    { "SIGUSR1", SIGUSR1, "user defined signal 1" },
    { "SIGSEGV", SIGSEGV, "segmentation violation" },
    { "SIGUSR2", SIGUSR2, "user defined signal 2" },
    { "SIGPIPE", SIGPIPE, "write on a pipe with no one to read it" },
    { "SIGALRM", SIGALRM, "alarm clock" },
    { "SIGTERM", SIGTERM, "software termination signal" },
    { "SIGCHLD", SIGCHLD, "child status has changed" },
    { "SIGCONT", SIGCONT, "continue" },
    { "SIGSTOP", SIGSTOP, "stop" },
    { "SIGTSTP", SIGTSTP, "stop signal generated from keyboard" },
    { "SIGTTIN", SIGTTIN, "background read attempted from control terminal" },
    { "SIGTTOU", SIGTTOU, "background write attempted to control terminal" },
    { "SIGURG", SIGURG, "urgent condition on I/O channel" },
    { "SIGXCPU", SIGXCPU, "cpu time limit exceeded" },
    { "SIGXFSZ", SIGXFSZ, "file size limit exceeded" },
    { "SIGVTALRM", SIGVTALRM, "virtual time alarm" },
    { "SIGPROF", SIGPROF, "profiling time alarm" },
    { "SIGWINCH", SIGWINCH, "window size change" },
    { "SIGIO", SIGIO, "I/O possible" },
    { "SIGSYS", SIGSYS, "bad argument to system call" },
};

void dbg_signal_text(int signo, const char *name, char *buf, int size)
{
    for (size_t i = 0; i < sizeof signals / sizeof signals[0]; i++)
        if ((name && strcmp(name, signals[i].name) == 0) ||
            (!name && signo == signals[i].signo)) {
            snprintf(buf, (size_t)size, "%d: %s (%s)", signals[i].signo,
                     signals[i].name, signals[i].text);
            return;
        }
    if (name)
        snprintf(buf, (size_t)size, "%s", name);
    else
        snprintf(buf, (size_t)size, "%d", signo);
}
