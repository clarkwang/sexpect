
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

int
main(int argc, char ** argv)
{
    double d;
    long l;
    struct timeval t;
    int ret;

    if (argc == 1) {
        printf("usage: sleep 1.23\r");
        return 1;
    }

    d = atof(argv[1]);
    if (d < 0) {
        return 1;
    }

    l = d * 1e6;
    t.tv_sec  = l / 1000000;
    t.tv_usec = l % 1000000;
    ret = select(1, NULL, NULL, NULL, & t);
    if (ret < 0) {
        perror("select");
        return 1;
    }

    return 0;
}
