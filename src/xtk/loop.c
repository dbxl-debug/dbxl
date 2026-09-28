/*
 * The event loop.  Single threaded: poll() on the X connection (backend
 * descriptors join in a later milestone), drain X events, dispatch each to
 * the registered handlers until one claims it.
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
    struct sigaction sa;
    struct pollfd pfd;

    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);

    pfd.fd = ConnectionNumber(dpy);
    pfd.events = POLLIN;

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
        if (poll(&pfd, 1, -1) < 0 && errno != EINTR)
            return 1;
    }
    return quit_status;
}
