#include "l8_common.h"

#define SPELL_TYPES 3
const char *spell_names[SPELL_TYPES] = {"Divination", "Summon Elemental", "Fireball"};
#define BOARD_SIZE 8
#define BACKLOG 16

#define MAX_QUEUE 10
#define THREAD_COUNT 3
#define FAMILIAR_DELAY 100

#define MAX_CLIENTS 2
#define MAX_NAME_LENGTH 14

typedef struct __attribute__((__packed__)) message
{
    char type;
    char pad;
    char body[14];
} msg_t;
typedef struct login
{
    char name[14];
} l_t;
typedef struct cast
{
    uint16_t spell;
    uint16_t x;
    uint16_t y;
} c_t;
typedef struct fifo
{
    msg_t casts[MAX_QUEUE];
    struct sockaddr_in addr[MAX_QUEUE];
    int top;
    int size;
    sem_t sem_add;
    sem_t sem_del;
    pthread_mutex_t mut;
} f_t;

typedef struct player
{
    struct sockaddr_in address;
    char name[14];
    int pebbles;
    int logged_in;
    pthread_mutex_t mut;
} p_t;
typedef struct targ
{
    f_t *fifo;
    int id;
    p_t *players;
    int **board;
    pthread_mutex_t *board_mut;

} targ_t;
typedef struct judge
{
    p_t *players;
    int isstarted;
    int sock;
    pthread_mutex_t mut;
    int **board;
    pthread_mutex_t *board_mut;
} j_t;

volatile sig_atomic_t work = 1;
void siginthandler(int signo)
{
    work = 0;
}
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
void msg_to_cast(c_t *cast, msg_t *msg)
{

    uint16_t tmp;
    memcpy(&tmp, msg->body, sizeof(uint16_t));
    cast->spell = ntohs(tmp);
    memcpy(&tmp, msg->body + 2, sizeof(uint16_t));
    cast->x = ntohs(tmp);
    memcpy(&tmp, msg->body + 4, sizeof(uint16_t));
    cast->y = ntohs(tmp);
}
void msg_to_login(l_t *login, msg_t *msg)
{
    strncpy(login->name, msg->body, 14);
}

void usage(char *name)
{
    printf("%s <in_port>\n", name);
    printf("  in_port - port that accepts messages\n");
    exit(EXIT_FAILURE);
}

int isunique(int player_count, p_t *players, struct sockaddr_in *addr)
{

    int i = 0;
    for (; i < player_count; i++)
    {
        pthread_cleanup_push(cleanup, (void *)&players[i].mut);
        pthread_mutex_safe_lock(&players[i].mut);

        if (addr->sin_port == players[i].address.sin_port && players[i].address.sin_addr.s_addr == addr->sin_addr.s_addr)
        {
            return -1;
        }
        pthread_cleanup_pop(1);
    }
    if (i == player_count && i < MAX_CLIENTS)
    {
        return i;
    }
    return -1;
}
int isfromplayer(int player_count, p_t *players, struct sockaddr_in *addr)
{
    for (int i = 0; i < player_count; i++)
    {
        pthread_cleanup_push(cleanup, (void *)&players[i].mut);
        pthread_mutex_safe_lock(&players[i].mut);

        if (players[i].address.sin_port == addr->sin_port && addr->sin_addr.s_addr == players[i].address.sin_addr.s_addr)
        {
            return i;
        }
        pthread_cleanup_pop(1);
    }
    return -1;
}
void deal_with_cast(msg_t *msg, int playerId, p_t *players, int **board, pthread_mutex_t *board_mut)
{
    c_t cast;
    msg_to_cast(&cast, msg);
    if (cast.x > BOARD_SIZE || cast.y > BOARD_SIZE || cast.spell > SPELL_TYPES)
    {
        puts("Wrong cast");
        return;
    }
    int cost;
    if (cast.spell == 0)
    {
        cost = 1;
    }
    else if (cast.spell == 1)
    {
        cost = 3;
    }
    else
    {
        cost = 4;
    }
    if (players[playerId].pebbles < cost)
    {
        printf("[tee hee] Not enough pebbles, %s!\n", players[playerId].name);
        ms_sleep(FAMILIAR_DELAY);
        return;
    }
    pthread_cleanup_push(cleanup, (void *)board_mut);
    pthread_mutex_safe_lock(board_mut);
    board[cast.x][cast.y] = playerId;
    pthread_cleanup_pop(1);
    printf("[Cast] %s casts %s, onto %d, %d\n", players[playerId].name, spell_names[cast.spell], cast.x, cast.y);
}
void deal_with_message(msg_t *msg, f_t *fifo, struct sockaddr_in *addr, int *player_count, p_t *players)
{
    if (*player_count < MAX_CLIENTS)
    {
        int uniq;
        if (msg->type == 'l' && (uniq = isunique(*player_count, players, addr)) >= 0)
        {
            pthread_cleanup_push(cleanup, (void *)&players[*player_count].mut);
            pthread_mutex_safe_lock(&players[*player_count].mut);

            players[*player_count].address = *addr;
            players[*player_count].logged_in = 1;
            snprintf(players[*player_count].name, 14, "%s", msg->body);

            printf("[Login] Welcome %s\n", players[*player_count].name);
            pthread_cleanup_pop(1);
            (*player_count)++;
        }
        else
        {
            printf("Rejected\n");
        }

        return;
    }
    else if (isfromplayer(*player_count, players, addr) < 0)
    {
        printf("Rejected\n");
        return;
    }

    if (msg->type == 'l')
    {
        puts("[Login] Rejected");
        return;
    }
    if (msg->type == 'c')
    {

        pthread_cleanup_push(cleanup, (void *)&fifo->mut);
        pthread_mutex_safe_lock(&fifo->mut);
        if (fifo->size >= MAX_QUEUE)
        {
            puts("Queue full");
        }
        else
        {
            fifo->casts[(fifo->top + fifo->size) % MAX_QUEUE] = *msg;
            fifo->addr[(fifo->top + fifo->size) % MAX_QUEUE] = *addr;
            fifo->size++;
            sem_post(&fifo->sem_del);
        }

        pthread_cleanup_pop(1);

        return;
    }
    if (msg->type == 'q')
    {
        int id = isfromplayer(*player_count, players, addr);
        work = 0;
        for (int i = 0; i < THREAD_COUNT; i++)
        {
            sem_post(&fifo->sem_del);
        }

        printf("[Quit] %s quit. Goodbye!\n", players[id].name);
        printf("-= Congratulations, %s, you win! =-\n", players[(id + 1) % MAX_CLIENTS].name);
        return;
    }
    puts("Incorrect message type");
}
void doServer(int socket, f_t *fifo, p_t *players, j_t *jarg)
{
    int received_bytes = 0;
    int player_count = 0;
    msg_t msg;
    struct sockaddr_in addr;
    socklen_t size = sizeof(addr);
    while (work)
    {
        if (player_count == MAX_CLIENTS)
        {
            pthread_cleanup_push(cleanup, (void *)&jarg->mut);
            pthread_mutex_safe_lock(&jarg->mut);
            jarg->isstarted = 1;
            pthread_cleanup_pop(1);
        }
        if ((received_bytes = recvfrom(socket, &msg, sizeof(msg_t), 0, &addr, &size)) < 0)
        {
            if (errno == EINTR)
            {
                for (int i = 0; i < THREAD_COUNT; i++)
                {
                    sem_post(&fifo->sem_del);
                }

                break;
            }
            ERR("recvfrom");
        }
        // printf("received %d bytes buffor %s ", received_bytes, &msg.body);

        if (received_bytes != 16)
        {
            puts("Malformed message");
            continue;
        }

        deal_with_message(&msg, fifo, &addr, &player_count, players);
    }
}
void init(targ_t *args, f_t *fifo, p_t *players, int **board, pthread_mutex_t *board_mut)
{
    for (int i = 0; i < THREAD_COUNT; i++)
    {

        args[i].fifo = fifo;
        args[i].id = i;
        args[i].players = players;
        args[i].board = board;
        args[i].board_mut = board_mut;
    }
}

void *threadWork(void *arg)
{
    targ_t *targ = (targ_t *)arg;
    f_t *fifo = targ->fifo;
    msg_t msg;

    struct sockaddr_in addr;
    while (work)
    {
        sem_wait(&fifo->sem_del);
        if (work == 0)
        {
            return NULL;
        }
        pthread_cleanup_push(cleanup, (void *)&fifo->mut);
        pthread_mutex_safe_lock(&fifo->mut);
        if (fifo->size < 0)
        {
            puts("fifo empty");
        }
        else
        {
            msg = fifo->casts[fifo->top];
            addr = fifo->addr[fifo->top];
            fifo->top = (fifo->top + 1) % MAX_QUEUE;
            fifo->size--;
        }
        pthread_cleanup_pop(1);
        ms_sleep(FAMILIAR_DELAY);
        int playerId = isfromplayer(MAX_CLIENTS, targ->players, &addr);

        deal_with_cast(&msg, playerId, targ->players, targ->board, targ->board_mut);
    }
    return NULL;
}
void *judgework(void *arg)
{
    j_t *jarg = (j_t *)arg;
    p_t *players = jarg->players;
    pthread_mutex_t *mut = &jarg->mut;
    while (work)
    {
        sleep(1);
        pthread_cleanup_push(cleanup, (void *)mut);
        pthread_mutex_safe_lock(mut);
        if (!jarg->isstarted)
        {
            goto jump;
        }
        for (int y = 0; y < BOARD_SIZE; y++)
        {
            for (int x = 0; x < BOARD_SIZE; x++)
            {
                printf("%d ", jarg->board[x][y]);
            }
            printf("\n");
        }

        for (int i = 0; i < MAX_CLIENTS; i++)
        {
        }
    jump:
        pthread_cleanup_pop(1);
    }
}
int main(int argc, char **argv)
{
    if (argc != 2)
    {
        usage(argv[0]);
    }
    sethandler(SIG_IGN, SIGPIPE);
    sethandler(siginthandler, SIGINT);
    int port = atoi(argv[1]);
    int socket = bind_inet_socket(port, SOCK_DGRAM, BACKLOG);
    if (socket < 0)
    {
        ERR("bind");
    }

    // fifo
    f_t fifo;
    if (sem_init(&fifo.sem_add, 0, 16) < 0 || sem_init(&fifo.sem_del, 0, 0) < 0)
    {
        ERR("sem init");
    }
    pthread_mutexattr_t mutattr;
    if (pthread_mutexattr_init(&mutattr) < 0 || pthread_mutexattr_setrobust(&mutattr, PTHREAD_MUTEX_ROBUST) < 0 ||
        pthread_mutex_init(&fifo.mut, &mutattr) < 0)
    {
        ERR("mutex inti");
    }
    fifo.size = 0;
    fifo.top = 0;

    // board
    int **board = malloc(sizeof(int *) * BOARD_SIZE);
    pthread_mutex_t board_mut;
    pthread_mutex_init(&board_mut, &mutattr);
    if (board == NULL)
    {
        ERR("malloc");
    }
    for (int i = 0; i < BOARD_SIZE; i++)
    {
        board[i] = malloc(sizeof(int) * BOARD_SIZE);
        if (board[i] == NULL)
        {
            ERR("malloc");
        }
        memset(board[i], 0, sizeof(int) * 128);
    }
    // players
    p_t players[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        players[i].logged_in = 0;
        players[i].name[0] = '\0';
        players[i].pebbles = 10;
        if (pthread_mutex_init(&players[i].mut, &mutattr) < 0)
        {
            ERR("mutexinit");
        }
    }
    // judge
    j_t jarg;
    jarg.isstarted = 0;
    if (pthread_mutex_init(&jarg.mut, &mutattr) < 0)
    {
        ERR("mutex init");
    }
    jarg.players = players;
    jarg.sock = socket;
    jarg.board = board;
    jarg.board_mut = &board_mut;

    // threads
    pthread_t judge;
    pthread_t threads[THREAD_COUNT];
    targ_t args[THREAD_COUNT];
    init(args, &fifo, players, board, &board_mut);
    if (pthread_create(&judge, NULL, judgework, (void *)&jarg) < 0)
    {
        ERR("thread create");
    }
    for (int i = 0; i < THREAD_COUNT; i++)
    {
        if (pthread_create(&threads[i], NULL, threadWork, (void *)&args[i]) < 0)
        {
            ERR("thread create");
        }
    }

    doServer(socket, &fifo, players, &jarg);
    for (int i = 0; i < THREAD_COUNT; i++)
    {
        pthread_join(threads[i], NULL);
    }
    pthread_join(judge, NULL);
    for (int i = 0; i < BOARD_SIZE; i++)
    {
        free(board[i]);
    }
    free(board);
}
