
/*
On March 1, 1815, to the surprise of the world, Napoleon Bonaparte landed with a small detachment at Juan in France. Received enthusiastically by the French, he quickly regained power. Thus began the period known today as “The Hundred Days”, which ended with the Battle of Waterloo.

In this task, we will simulate delivering reports to Napoleon’s headquarters during the battle. Reports are carried by messengers. In the chaos of battle, it often happens that a messenger fails to arrive. Therefore, we will simulate report delivery using the UDP protocol.

For testing, you can use the netcat program with the -u flag.
Stages #

    The server program takes one argument: the port number.
    The program waits for datagrams on the given port. Messages have the form <X> <Y> <P> <division name>. X and Y are map coordinates (natural numbers in the range from 0 to 99), while <division name> is text no longer than 128 characters. P indicates the division’s allegiance and may be 0 (enemy) or 1 (allied). The message means that the given division (allied or enemy) has moved to the given position. After receiving a datagram, parse the message and print to the terminal a message of the form <our/enemy> division <division name> was seen at position <X>:<Y>. If the message is malformed, print an error message, but do not terminate the program.

    In such a fierce battle, there is considerable confusion even within headquarters itself. Incoming messengers throw reports onto a stack by the entrance, from where they are picked up by four adjutants, who use them to update the headquarters maps. Implement a thread pool of adjutants. After receiving a message, the server adds it to the stack (of size STACK_SIZE equal to 16). The adjutant threads wait for a new message — the one that receives it performs the parsing and prints the message as in the first stage. Use a mutex for synchronization (to protect the stack) and a semaphore or condition variable (for the pointer to the top of the stack).

    Add updates to the headquarters maps. Create a shared array of division names among the threads, of size DIVISION_NAMES_SIZE equal to 128. After receiving a new report, an adjutant works on it (that is, sleeps for 10 ms). Then they check whether the division name is already in the array. If not, they append it to the end (remember synchronization!). Add a shared headquarters map — a two-dimensional array of size 100x100, initially filled with -1. The adjutant updates the division’s position on the map — that is, they look up its number, set its previous field to -1, and then write the division number (its index in the division names array) to the coordinates from the message. To ensure synchronization, add one mutex per map row.

    Add Napoleon’s thread. Remember the addresses from which the last report for a given division arrived. Every 30 ms, the Emperor of the French prints the state of the map. Then he chooses a random allied division and sends it an order of the form <X> <Y> <P> <division name>.*/
#include "l8_common.h"
#define MAXADDR 10
#define THREADS 4
typedef struct
{
    int x;
    int y;
    int p;
    char division[129];

} umsg_t;
typedef struct
{
    int id;
    int *idlethreads;
    int *condition;
    pthread_cond_t *cond;
    pthread_mutex_t *mutex;
} thread_arg;
volatile sig_atomic_t dowork = 1;
void cleanup(void *arg) { pthread_mutex_unlock((pthread_mutex_t *)arg); }
void siginthandler(int signo)
{
    dowork = 0;
}
struct connections
{
    int free;
    int32_t chunkNo;
    struct sockaddr_in addr;
};

int msg_to_usable(char buffer[], umsg_t *umsg, int received_bytes)
{
    if (buffer[received_bytes - 1] == '\n')
    {
        received_bytes--;
    }
    buffer[received_bytes] = '\0';

    char *tok = strtok(buffer, " ");
    int cnt = 0;
    while (tok != NULL)
    {
        if (cnt == 0)
        {
            umsg->x = atoi(tok);
        }
        else if (cnt == 1)
        {
            umsg->y = atoi(tok);
        }
        else if (cnt == 2)
        {
            umsg->p = atoi(tok);
        }
        else if (cnt == 3)
        {
            strncpy(umsg->division, tok, 128);
        }
        cnt++;
        tok = strtok(NULL, " ");
    }
    if (cnt <= 3 || umsg->x > 99 || umsg->y > 99 || umsg->p < 0 || umsg->p > 1 || umsg->y < 0 || umsg->x < 0)
    {
        return -1;
    }
    return 1;
}
void doServer(int socket)
{
    // sigset_t mask, oldmask;
    // sigemptyset(&mask);
    // sigaddset(&mask, SIGINT);
    // pthread_sigmask(SIG_BLOCK, &mask, &oldmask);

    char buffer[136];
    umsg_t umsg;
    struct sockaddr_in addr;
    // struct connections con[MAXADDR];
    // for (int i = 0; i < MAXADDR; i++)
    // {
    //     con[i].free = 0;
    // }

    while (dowork)
    {
        socklen_t size = sizeof(addr);
        int received_bytes;
        if ((received_bytes = recvfrom(socket, &buffer, sizeof(buffer), 0, &addr, &size)) < 0)
        {
            ERR("read");
        }
        if (msg_to_usable(buffer, &umsg, received_bytes) == -1)
        {
            printf("Malformed message\n");
            continue;
        }

        printf("%s division %s has been seen at %d %d\n", umsg.p == 0 ? "enemy" : "our", umsg.division, umsg.x, umsg.y);
    }
}
void thread_work(void *arg)
{
}
int main(int argc, char **argv)
{
    if (argc != 2)
    {
        exit(EXIT_FAILURE);
    }
    int idlethreads = 0;
    int condition = 0;
    int id = 0;
    pthread_t threads[THREADS];
    thread_arg targ[THREADS];
    pthread_cond_t cond;
    pthread_cond_init(&cond, )

        sethandler(siginthandler, SIGINT);
    sethandler(SIG_IGN, SIGPIPE);
    int port_num = atoi(argv[1]);
    int socket = bind_inet_socket(port_num, SOCK_DGRAM, 10);
    doServer(socket);
}