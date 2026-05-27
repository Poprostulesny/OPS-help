
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
#include <time.h>
#define MAXADDR 10
#define THREADS 4
#define STACK_SIZE 16
typedef struct
{
    int x;
    int y;
    int p;
    char division[129];

} umsg_t;

struct connections
{
    int free;
    int32_t chunkNo;
    struct sockaddr_in addr;
};

typedef struct estack
{
    char buffer[136];
    int received;
    struct sockaddr_in addr;
    struct estack *next;
} estack;
typedef struct
{

    pthread_mutex_t mut;
    pthread_cond_t cond;
    sem_t sem_add;
    estack *top;
    int cnt;
} stack;

typedef struct
{
    int id;
    int *idlethreads;
    int *condition;
    pthread_cond_t *cond;
    pthread_mutex_t *mutex;
    stack *stack;
    char **division_names;
    pthread_mutex_t *div_mut;
    pthread_mutex_t *map_mut;
    int **map;
    int *names;
    struct sockaddr_in *addresses;
    pthread_mutex_t *addr_mut;
    int sock;

} thread_arg;

volatile sig_atomic_t dowork = 1;
void cleanup(void *arg)
{
    pthread_mutex_unlock((pthread_mutex_t *)arg);
}

void pthread_mutex_safe_lock(pthread_mutex_t *mut)
{
    if (pthread_mutex_lock(mut) < 0)
    {
        if (errno = EOWNERDEAD)
            pthread_mutex_consistent(mut);
        else
            ERR("mutex lock");
    }
}
void msleep(unsigned int milisec)
{
    time_t sec = (int)(milisec / 1000);
    milisec = milisec - (sec * 1000);
    struct timespec req = {0};
    req.tv_sec = sec;
    req.tv_nsec = milisec * 1000000L;
    if (nanosleep(&req, &req))
        ERR("nanosleep");
}
void siginthandler(int signo)
{
    dowork = 0;
}

int msg_to_usable(char buffer[], umsg_t *umsg, int received_bytes)
{
    if (buffer[received_bytes - 1] == '\n')
    {
        received_bytes--;
    }
    buffer[received_bytes] = '\0';
    int n = 0;
    if (sscanf(buffer, "%d %d %d %n", &umsg->x, &umsg->y, &umsg->p, &n) < 0)
    {
        return -1;
    }
    strncpy(umsg->division, buffer + n, 128);
    buffer[128] = '\0';
    if (strlen(umsg->division) == 0)
    {
        return -1;
    }
    if (umsg->x > 99 || umsg->y > 99 || umsg->p < 0 || umsg->p > 1 || umsg->y < 0 || umsg->x < 0)
    {
        return -1;
    }
    return 1;
}
void doServer(int socket, stack *st)
{
    // sigset_t mask, oldmask;
    // sigemptyset(&mask);
    // sigaddset(&mask, SIGINT);
    // pthread_sigmask(SIG_BLOCK, &mask, &oldmask);

    char buffer[136];

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
        if ((received_bytes = recvfrom(socket, &buffer, sizeof(buffer) - 1, 0, &addr, &size)) < 0)
        {
            if (errno == EINTR)
            {
                break;
            }
            ERR("read");
        }
        if (dowork == 0)
        {
            break;
        }
        buffer[received_bytes] = '\0';
        sem_wait(&st->sem_add);
        pthread_mutex_safe_lock(&st->mut);
        estack *newel = malloc(sizeof(estack));
        buffer[135] = '\0';
        strcpy(newel->buffer, buffer);
        newel->next = st->top;
        newel->addr = addr;
        newel->received = received_bytes;
        st->top = newel;
        st->cnt++;

        printf("added to stack %s\n", buffer);
        pthread_cond_signal(&st->cond);
        puts("wakup");
        pthread_mutex_unlock(&st->mut);
    }
    pthread_cond_broadcast(&st->cond);
}
void work(estack *stel, thread_arg *targ)
{
    umsg_t umsg;

    if (msg_to_usable(stel->buffer, &umsg, stel->received) == -1)
    {
        printf("Malformed message\n");
        return;
    }
    int ind = 0;
    printf("%s division %s has been seen at %d %d\nbeginning to work on it\n", umsg.p == 0 ? "enemy" : "our", umsg.division, umsg.x, umsg.y);
    msleep(10);
    pthread_cleanup_push(cleanup, (void *)targ->div_mut);
    pthread_mutex_safe_lock(targ->div_mut);

    for (; ind < *targ->names; ind++)
    {
        if (strcmp(umsg.division, targ->division_names[ind]) == 0)
        {
            goto a;
        }
    }
    snprintf(targ->division_names[ind], 129, "%s", umsg.division);
    (*targ->names)++;
a:
    pthread_cleanup_pop(1);

    pthread_cleanup_push(cleanup, (void *)targ->map_mut);
    pthread_mutex_safe_lock(targ->map_mut);
    for (int x = 0; x < 100; x++)
    {
        for (int y = 0; y < 100; y++)
        {
            if (targ->map[x][y] == ind)
            {
                targ->map[x][y] = -1;
                goto b;
            }
        }
    }
b:
    targ->map[umsg.x][umsg.y] = ind;
    pthread_cleanup_pop(1);
    pthread_cleanup_push(cleanup, (void *)targ->addr_mut);
    pthread_mutex_safe_lock(targ->addr_mut);
    targ->addresses[ind] = stel->addr;
    pthread_cleanup_pop(1);
}
void *napoleon(void *arg)
{
    thread_arg targ;
    unsigned int seed = getpid();
    memcpy(&targ, arg, sizeof(targ));
    while (dowork)
    {
        msleep(30);

        pthread_cleanup_push(cleanup, (void *)targ.map_mut);
        pthread_mutex_safe_lock(targ.map_mut);
        for (int y = 0; y < 100; y++)
        {
            for (int x = 0; x < 100; x++)
            {
                printf("%d ", targ.map[x][y]);
            }
            printf("\n");
        }
        pthread_cleanup_pop(1);
        pthread_cleanup_push(cleanup, (void *)targ.div_mut);
        pthread_mutex_safe_lock(targ.div_mut);
        if (*targ.names < 0)
        {
            goto k;
        }
        char buff[136];

        pthread_cleanup_push(cleanup, (void *)targ.addr_mut);
        pthread_mutex_safe_lock(targ.addr_mut);
        int id = rand_r(&seed) % (*targ.names);
        int n = snprintf(buff, 136, "%d %d %s", rand_r(&seed) % 100, rand_r(&seed) % 100, targ.names[id]);
        sendto(targ.sock, buff, n, 0,
               (struct sockaddr *)&targ.addresses[id], sizeof(targ.addresses[id]));
        pthread_cleanup_pop(1);
    k:
        pthread_cleanup_pop(1);
    }
    return NULL;
}
void *thread_work(void *arg)
{
    thread_arg targ;
    memcpy(&targ, arg, sizeof(targ));
    estack *stel;
    while (1)
    {
        printf("Thread %d waiting for tasks\n", targ.id);
        pthread_cleanup_push(cleanup, (void *)targ.mutex);
        pthread_mutex_safe_lock(targ.mutex);

        (*targ.idlethreads)++;
        while (*targ.condition <= 0 && dowork)
            if (pthread_cond_wait(targ.cond, targ.mutex) != 0)
                ERR("pthread_cond_wait");
        if (!dowork)
            pthread_exit(NULL);
        printf("Thread %d taking assignment from stack\n", targ.id);
        (*targ.idlethreads)--;
        (*targ.condition)--;
        stel = targ.stack->top;
        targ.stack->top = stel->next;

        sem_post(&targ.stack->sem_add);
        pthread_cleanup_pop(1);
        printf("Thread %d starting work on buffer %s\n", targ.id, stel->buffer);
        work(stel, &targ);
        free(stel);
    }
    return NULL;
}
void init(thread_arg *args, pthread_t *threads, int id, int *idle_threads, stack *st,
          char **division_names, int **map, pthread_mutex_t *mapmut, pthread_mutex_t *div_mut, int *names,
          struct sockaddr_in addresses[], pthread_mutex_t *addr_mut, int socket)
{

    for (int i = 0; i < THREADS; i++)
    {
        args[i].id = i;
        args[i].idlethreads = idle_threads;
        args[i].condition = &st->cnt;
        args[i].mutex = &st->mut;
        args[i].stack = st;
        args[i].cond = &st->cond;
        args[i].division_names = division_names;
        args[i].div_mut = div_mut;
        args[i].map = map;
        args[i].names = names;
        args[i].map_mut = mapmut;
        args[i].addresses = addresses;
        args[i].addr_mut = addr_mut;
        args[i].sock = socket;
        pthread_create(&threads[i], NULL, thread_work, &args[i]);
    }
}
int main(int argc, char **argv)
{
    if (argc != 2)
    {
        exit(EXIT_FAILURE);
    }
    struct sockaddr_in addresses[128];
    int idlethreads = 0;
    int names = 0;
    char **division_names = malloc(sizeof(char *) * 128);
    int **map = malloc(sizeof(int *) * 100);
    if (division_names == NULL || map == NULL)
    {
        ERR("malloc");
    }
    for (int i = 0; i < 128; i++)
    {
        if ((division_names[i] = malloc(129 * sizeof(char))) == NULL)
        {
            ERR("malloc");
        }
    }
    for (int i = 0; i < 100; i++)
    {
        if ((map[i] = malloc(100 * sizeof(int))) == NULL)
        {
            ERR("malloc");
        }
        for (int y = 0; y < 100; y++)
        {
            map[i][y] = -1;
        }
    }
    int id = 0;
    pthread_t threads[THREADS];
    thread_arg targ[THREADS];

    sethandler(siginthandler, SIGINT);
    sethandler(SIG_IGN, SIGPIPE);
    int port_num = atoi(argv[1]);
    int socket = bind_inet_socket(port_num, SOCK_DGRAM, 10);

    stack st;
    st.cnt = 0;
    st.top = NULL;

    pthread_mutexattr_t mutattr;
    pthread_mutexattr_init(&mutattr);
    pthread_mutexattr_setrobust(&mutattr, PTHREAD_MUTEX_ROBUST);
    pthread_mutex_t map_mut, div_mut, addr_mut;
    pthread_mutex_init(&st.mut, &mutattr);
    sem_init(&st.sem_add, 0, STACK_SIZE);
    pthread_cond_init(&st.cond, NULL);
    pthread_mutex_init(&div_mut, &mutattr);
    pthread_mutex_init(&map_mut, &mutattr);
    pthread_mutex_init(&addr_mut, &mutattr);
    init(targ, threads, id, &idlethreads, &st, division_names, map, &map_mut,
         &div_mut, &names, addresses, &addr_mut, socket);

    doServer(socket, &st);

    for (int i = 0; i < THREADS; i++)
    {
        pthread_join(threads[i], NULL);
    }
    estack *elem;
    while (st.top != NULL)
    {
        elem = st.top;
        st.top = st.top->next;
        free(elem);
    }
    for (int i = 0; i < 128; i++)
    {
        free(division_names[i]);
    }
    free(division_names);
}