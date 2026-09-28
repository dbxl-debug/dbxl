/*
 * The event loop.  Single threaded: poll() on the X connection and the
 * registered descriptors (the debugger backend), drain X events, dispatch
 * each to the registered handlers until one claims it, and call a
 * descriptor's callback when it is readable.
 */
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

#include "xtk/xtk.h"

#define MAX_HANDLERS 64

static struct {
    xtk_event_fn fn;
    void *arg;
} handlers[MAX_HANDLERS];
static int nhandlers;

#define MAX_FDS 8

static struct {
    int fd;
    xtk_fd_fn fn;
    void *arg;
} fds[MAX_FDS];
static int nfds;

static volatile sig_atomic_t got_signal;
static bool quit;
static int quit_status;

static void on_signal(int sig)
{
    got_signal = sig;
}

void xtk_loop_add_handler(xtk_event_fn fn, void *arg)
{
    if (nhandlers == MAX_HANDLERS)
        abort();
    handlers[nhandlers].fn = fn;
    handlers[nhandlers].arg = arg;
    nhandlers++;
}

void xtk_loop_add_fd(int fd, xtk_fd_fn fn, void *arg)
{
    if (nfds == MAX_FDS)
        abort();
    fds[nfds].fd = fd;
    fds[nfds].fn = fn;
    fds[nfds].arg = arg;
    nfds++;
}

void xtk_loop_remove_fd(int fd)
{
    for (int i = 0; i < nfds; i++)
        if (fds[i].fd == fd) {
            fds[i] = fds[--nfds];
            return;
        }
}

void xtk_loop_quit(int status)
{
    quit = true;
    quit_status = status;
}

static void dispatch(const XEvent *ev)
{
    for (int i = 0; i < nhandlers; i++)
        if (handlers[i].fn(ev, handlers[i].arg))
            return;
}

int xtk_loop_run(void)
{
    Display *dpy = xtk_dpy();
    int n;
    struct sigaction sa;
    struct pollfd pfd[1 + MAX_FDS];

    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);

    while (!quit) {
        XFlush(dpy);
        while (!quit && XPending(dpy)) {
            XEvent ev;
            XNextEvent(dpy, &ev);
            dispatch(&ev);
        }
        if (got_signal)
            xtk_loop_quit(128 + got_signal);
        if (quit)
            break;
        XFlush(dpy);
        n = nfds;
        pfd[0].fd = ConnectionNumber(dpy);
        pfd[0].events = POLLIN;
        for (int i = 0; i < n; i++) {
            pfd[1 + i].fd = fds[i].fd;
            pfd[1 + i].events = POLLIN;
        }
        if (poll(pfd, (nfds_t)(1 + n), -1) < 0) {
            if (errno != EINTR)
                return 1;
            continue;
        }
        /* Callbacks may add or remove descriptors; match by fd. */
        for (int i = 0; i < n; i++) {
            if (!(pfd[1 + i].revents & (POLLIN | POLLHUP | POLLERR)))
                continue;
            for (int k = 0; k < nfds; k++)
                if (fds[k].fd == pfd[1 + i].fd) {
                    fds[k].fn(fds[k].fd, fds[k].arg);
                    break;
                }
        }
    }
    return quit_status;
}
