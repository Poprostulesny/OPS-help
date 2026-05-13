#include "l7-common.h"

void usage(char *name)
{
    printf("%s <timeout>\n", name);
    printf("  timeout - max waiting time after receiving the last message/connection (in seconds)\n");
    exit(EXIT_FAILURE);
}

#define SWAP(a, b)                     \
    do                                 \
    {                                  \
        __typeof__(a) __a = (a);       \
        __typeof__(b) __b = (b);       \
        __typeof__(*__a) __tmp = *__a; \
        *__a = *__b;                   \
        *__b = __tmp;                  \
    } while (0)

#define MAX_CLIENTS 10
#define MAX_PAIRS 3
#define UNIX_SK_NAME "Laurenty"
#define MAX_MSG_LEN 63

volatile sig_atomic_t do_work = 1;

void handle(int signo)
{
    do_work = 0;
}

int find_free_pair_id(char pairs[][2][MAX_MSG_LEN + 1])
{

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (strcmp(pairs[i][0], "??") == 0 && strcmp(pairs[i][1], "??") == 0)
        {
            return i;
        }
    }
    return -1;
}
int find_pair_with_first(char pairs[][2][MAX_MSG_LEN + 1], char *client)
{

    for (int i = 0; i < MAX_PAIRS; i++)
    {
        if (strcmp(pairs[i][0], client) == 0)
        {
            return i;
        }
    }
    return -1;
}

int find_matching_pair(char pairs[][2][MAX_MSG_LEN + 1], char *client, char *beloved)
{

    for (int i = 0; i < MAX_PAIRS; i++)
    {
        if (strcmp(pairs[i][0], beloved) == 0 && strcmp(pairs[i][1], client) == 0)
        {
            snprintf(pairs[i][0], MAX_MSG_LEN + 1, "%s", "??");
            snprintf(pairs[i][1], MAX_MSG_LEN + 1, "%s", "??");
            return i;
        }
    }
    return -1;
}
int find_id_name(char client_names[][MAX_MSG_LEN + 1], char *buffer)
{

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (strcmp(buffer, client_names[i]) == 0)
        {
            return i;
        }
    }
    return -1;
}
void receive_msg(int fd, char buffer[], int buf_len, int clients[], int maxclients, char client_names[MAX_CLIENTS][MAX_MSG_LEN + 1], char pairs[][2][MAX_MSG_LEN + 1], int epoll_fd)
{
    int id = find_client_id(fd, clients, maxclients);

    int r = read_from_fd(fd, buffer, buf_len, epoll_fd, clients, maxclients);
    if (r == 0)
    {
        if (find_pair_with_first(pairs, client_names[id]) < 0)
        {
            printf("I lost contact with client %s\n", client_names[id]);
        }
        snprintf(client_names[id], MAX_MSG_LEN, "??");
        close(fd);
        return;
    }
    if (r < 0)
        return;

    if (strcmp(client_names[id], "??") == 0)
    {
        snprintf(client_names[id], MAX_MSG_LEN, "%s", buffer);
    }
    else
    {
        int fp = find_free_pair_id(pairs);
        if (fp < 0)
        {
            return;
        }
        snprintf(pairs[fp][0], MAX_MSG_LEN + 1, "%s", client_names[id]);
        snprintf(pairs[fp][1], MAX_MSG_LEN + 1, "%s", buffer);
        printf("%s wants to marry %s\n", client_names[id], buffer);
        int mp = find_matching_pair(pairs, client_names[id], buffer);
        if (mp > 0)
        {
            snprintf(pairs[fp][0], MAX_MSG_LEN + 1, "%s", "??");
            snprintf(pairs[fp][1], MAX_MSG_LEN + 1, "%s", "??");
            printf("%s and %s got married!\n", client_names[id], buffer);
            snprintf(client_names[id], MAX_MSG_LEN, "%s", "??");
            // close sender
            char msg[2 * MAX_MSG_LEN + 1];
            int n = snprintf(msg, sizeof(msg), "Congratulations, %s and %s!", client_names[id], buffer);
            write_to_fd(fd, msg, n, epoll_fd, clients, MAX_CLIENTS);
            delete_from_epoll(epoll_fd, fd);
            clients[id] = -1;
            close(fd);
            // close beloved
            int idb = find_id_name(client_names, buffer);
            if (idb > 0)
            {
                write_to_fd(fd, msg, n, epoll_fd, clients, MAX_CLIENTS);
                delete_from_epoll(epoll_fd, clients[idb]);
                clients[idb] = -1;
                close(clients[idb]);
                snprintf(client_names[idb], MAX_MSG_LEN, "%s", "??");
            }
        }
    }
}
void server(int tcp_socket, int timeout)
{
    int epoll_fd = create_epoll_init(tcp_socket);
    struct epoll_event events[MAX_CLIENTS];
    int clients[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        clients[i] = -1;
    }
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);

    char buffer[MAX_MSG_LEN + 1];
    char pairs[MAX_PAIRS][2][MAX_MSG_LEN + 1];
    char client_names[MAX_CLIENTS][MAX_MSG_LEN + 1];
    for (int i = 0; i < MAX_PAIRS; i++)
    {
        snprintf(pairs[i][0], MAX_MSG_LEN + 1, "%s", "??");
        snprintf(pairs[i][1], MAX_MSG_LEN + 1, "%s", "??");
        snprintf(client_names[i], MAX_MSG_LEN + 1, "%s", "??");
    }

    while (do_work)
    {
        int nfds = epoll_pwait(epoll_fd, events, MAX_CLIENTS, timeout * 1000, &oldmask);
        if (nfds > 0)
        {
            for (int i = 0; i < nfds; i++)
            {
                int fd = events[i].data.fd;
                if (fd == tcp_socket)
                {
                    // new connection
                    int client_fd = add_new_client(fd);
                    if (add_or_reject_client(epoll_fd, clients, MAX_CLIENTS, client_fd) >= 0)
                    {
                        printf("Another young person %d needs my help!\n", client_fd);
                    }
                }
                else
                {

                    receive_msg(fd, buffer, MAX_MSG_LEN, clients, MAX_CLIENTS, client_names, pairs, epoll_fd);
                }
            }
        }
        if (nfds == 0)
        {
            do_work = 0;
        }
        if (nfds < 0 && errno != EINTR)
        {
            ERR("epoll_pwait");
        }
    }
    printf("Noone needs my help anymore!");
    close_all_clients(clients, MAX_CLIENTS);
    close(epoll_fd);
}

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
    sethandler(handle, SIGINT);

    int tcp_socket = bind_local_socket(UNIX_SK_NAME, 10);
    if (tcp_socket == -1)
    {
        ERR("bind");
    }
    set_nonblocking(tcp_socket);

    server(tcp_socket, timeout);

    close(tcp_socket);
    unlink(UNIX_SK_NAME);

    return EXIT_SUCCESS;
}
