#include "l7-common.h"

void usage(char *name)
{
    printf("%s <timeout>\n", name);
    printf("  timeout - max waiting time after receiving the last message/connection (in seconds)\n");
    exit(EXIT_FAILURE);
}

#define SWAP(a, b)                      \
    do                                  \
    {                                   \
        __typeof__(a) __a = (a);        \
        __typeof__(b) __b = (b);        \
        __typeof__(*__a) __tmp = *__a;  \
        *__a = *__b;                    \
        *__b = __tmp;                   \
    } while (0)

#define MAX_CLIENTS 10
#define MAX_PAIRS 3
#define UNIX_SK_NAME "Laurenty"
#define MAX_MSG_LEN 63

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    int timeout = atoi(argv[1]);
    if (timeout < 1)
    {
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    sethandler(SIG_IGN, SIGPIPE);

    return EXIT_SUCCESS;
}
