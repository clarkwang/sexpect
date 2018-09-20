
#include <stdio.h>
#include <stdlib.h>
#include <common.h>

int
main()
{
    char * pos_cases[] = {
        "",
        "^$",
        "^foo$",
        "^foo",
        "foo$",
        "123?456*789",
        ".+|^$<>(){}",
        "[]]",
        "[Pp]assword",
        "[^]]",
        "[!]]",
        "[a-z]",
        "[a-z]\\]",
    };
    char * neg_cases[] = {
        "[",
        "[a-z",
        "[a-z]]",
        "]",
        "\\",
        "\\^",
        "\\x",
        "[[:digit:]]",
        "[[.cC.]]",
        "[[=o=]]",
    };
    char * fr = NULL, * to = NULL;
    int i;

    printf("pos_cases:\n");
    for (i = 0; i < ARRAY_SIZE(pos_cases); ++i) {
        fr = pos_cases[i];
        to = glob2re(fr, & to, NULL);
        if (to == NULL) {
            exit(1);
        }
        printf("%20s  ->  %s\n", fr, to);
        free(to);
        to = NULL;
    }

    printf("neg_cases:\n");
    for (i = 0; i < ARRAY_SIZE(neg_cases); ++i) {
        fr = neg_cases[i];
        to = glob2re(fr, & to, NULL);
        if (to != NULL) {
            printf("%20s  ->  %s\n", fr, to);
            exit(1);
        }
        printf("%20s  ->  NULL\n", fr);
    }

    return 0;
}
