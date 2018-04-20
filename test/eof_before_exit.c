
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

int
main(int argc, char ** argv)
{
    int nsleep = 120;

    if (argc == 2) {
        nsleep = atoi(argv[1]);
    }

    printf("Ignoring SIGHUP...\n");
    signal(SIGHUP, SIG_IGN);

    printf("take a 10s nap\n");
    sleep(10);
    printf("take a 10s nap ... done\n");

    printf("Now close fd 0, 1, 2 and sleep %d seconds...\n", nsleep);
    close(0);
    close(1);
    close(2);

    sleep(nsleep);

    return 7;
}
