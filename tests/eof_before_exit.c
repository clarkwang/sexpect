
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
    int nsleep;
    int nexit;

    nsleep = atoi(argv[1]);
    nexit = atoi(argv[2]);

    /* When getting EOF, the server would close the pty master. And then SIGHUP
     * "is sent to the controlling process (session leader) associated with a
     * controlling terminal if a disconnect is detected by the terminal
     * interface."
     */
    printf("Ignoring SIGHUP...\n");
    sig_handle(SIGHUP, SIG_IGN);

    sleep(1);

    printf("Now close fd 0, 1, 2 and sleep %d seconds...\n", nsleep);
    close(0);
    close(1);
    close(2);

    sleep(nsleep);

    return nexit;
}
