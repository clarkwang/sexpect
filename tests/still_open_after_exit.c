
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

void
sig_handle(int signo, void (*handler)(int) )
{
    struct sigaction act = { 0 };

    act.sa_handler = handler;
    sigaction(signo, &act, NULL);
}

int
main(int argc, char ** argv)
{
    pid_t pid;
    int t1, delta;
    int nexit;

    t1 = atoi(argv[1]);
    delta = atoi(argv[2]);
    nexit = atoi(argv[3]);

    pid = fork();
    if (pid < 0) {
        exit(1);
    } else if (pid > 0) {
        /* [<] parent */

        printf("parent sleeping...\n");
        sleep(t1);
        printf("parent exiting...\n");

        exit(nexit);
    } else {
        /* [<] child */

        /* When the session leader terminates, SIGHUP is sent to each process
         * in the foreground process group. So child needs to ignore SIGHUP or
         * it'll killed.
         */
        printf("Ignoring SIGHUP...\n");
        sig_handle(SIGHUP, SIG_IGN);

        printf("child sleeping...\n");
        sleep(t1 + delta);
        printf("child exiting...\n");
    }

    return 0;
}
