/*
 * A test program for milestone 6: forks (Fork path), a crash (-q, signals)
 * and a long sleep to attach to (-a).
 *   forky        forks; the child returns 3, the parent 0
 *   forky crash  dies of SIGSEGV
 *   forky sleep  lets any process attach, then sleeps until signalled
 */
#include <stdio.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>

int generation = 0;

static int child(void)
{
    generation = 1;
    return 3;
}

int main(int argc, char **argv)
{
    pid_t p;

    if (argc > 1 && argv[1][0] == 'c')
        *(volatile int *)0 = 1;
    if (argc > 1 && argv[1][0] == 's') {
        prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0);
        for (;;)
            pause();
    }
    p = fork();
    if (p == 0)
        return child();
    waitpid(p, NULL, 0);
    return 0;
}
