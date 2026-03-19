#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <string.h>
#include <unistd.h>

#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

#define MAX_GRAPH_NODES 32
#define MAX_PATH_LENGTH (2 * MAX_GRAPH_NODES)
#define FIFO_NAME "/tmp/colony_fifo"

volatile sig_atomic_t stop_work;
int set_handler(void (*f)(int), int sig)
{
    struct sigaction act = {0};
    act.sa_handler = f;
    if (sigaction(sig, &act, NULL) == -1)
        return -1;
    return 0;
}

void sig_handler(int signal)
{
    if (signal == SIGINT)
    {
        stop_work = 1;
    }
}

void msleep(int ms)
{
    struct timespec tt;
    tt.tv_sec = ms / 1000;
    tt.tv_nsec = (ms % 1000) * 1000000;
    while (nanosleep(&tt, &tt) == -1)
    {
    }
}

void usage(int argc, char *argv[])
{
    printf("%s graph start dest\n", argv[0]);
    printf("  graph - path to file containing colony graph\n");
    printf("  start - starting node index\n");
    printf("  dest - destination node index\n");
    exit(EXIT_FAILURE);
}
void freegraph(int **graph)
{
    for (int i = 0; i < MAX_GRAPH_NODES; i++)
    {
        free(graph[i]);
    }
    free(graph);
}
void loadGraph(int **graph, char *name, int *s)
{
    FILE *graphfile = fopen(name, "r");
    if (graphfile == NULL)
    {
        ERR("fopen");
    }
    int n;
    fscanf(graphfile, "%d", &n);
    int a, b;
    int i;
    while (fscanf(graphfile, "%d %d", &a, &b) > 0)
    {
        i++;
        graph[a][b] = 1;
    }
    printf("lines %d\n", i);
    *s = n;
}
typedef struct node
{
    int fd[2];
    int writeends[MAX_GRAPH_NODES];
    int numneighbors;
    int id;

} node_t;
void createNodes(int **graph, node_t *nodes, int n)
{

    if (nodes == NULL)
    {
        ERR("mallco");
    }
    for (int i = 0; i < n; i++)
    {
        pipe(nodes[i].fd);
        nodes[i].id = i;
    }
    for (int i = 0; i < n; i++)
    {
        int numneigh = 0;
        for (int y = 0; y < n; y++)
        {
            if (graph[i][y] == 1)
            {
                numneigh++;
                nodes[i].writeends[y] = nodes[y].fd[1];
            }
        }
        nodes[i].numneighbors = numneigh;
    }
}
typedef struct nodearg
{
    int **graph;
    node_t node;

} narg_t;

typedef struct ant
{
    int id;
    int dest;
    int path_len;
    int path[64];
} ant_t;
void NodeProcess(void *arg)
{
    set_handler(sig_handler, SIGINT);
    srand(time(NULL) ^ getpid());
    int fd = open(FIFO_NAME, O_WRONLY | O_NONBLOCK);
    int **graph = ((narg_t *)arg)->graph;
    node_t node = ((narg_t *)arg)->node;
    char buf[100];
    int r = sprintf(buf, "Node %d:", node.id);
    int neigh_num = 0;
    for (int i = 0; i < MAX_GRAPH_NODES; i++)
    {
        if (graph[node.id][i])
        {
            r += sprintf(buf + r, " %d", i);
            neigh_num++;
        }
    }

    puts(buf);
    ant_t ant;
    while (!stop_work && read(node.fd[0], &ant, sizeof(ant)) > 0)
    {
        msleep(100);
        if (neigh_num == 0 || ant.path_len == 64)
        {
            printf("Ant %d: got lost\n", ant.id);
            continue;
        }
        else if (ant.dest == node.id)
        {
            printf("Ant %d: found food\n", ant.id);
            ant.path[ant.path_len] = node.id;
            ant.path_len++;
            write(fd, &ant, sizeof(ant));
            continue;
        }
        int forw = rand() % neigh_num;

        for (int i = 0, y = 0; !stop_work && i < MAX_GRAPH_NODES; i++)
        {
            if (graph[node.id][i])
            {
                if (y == forw)
                {
                    ant.path[ant.path_len] = node.id;
                    ant.path_len++;
                    if (!stop_work && write(node.writeends[i], &ant, sizeof(ant)) <= 0)
                    {
                        printf("Ant %d: got lost\n", ant.id);
                        break;
                    }
                    int col = rand() % 50;
                    if (col == 0)
                    {
                        printf("Node %d: collapsed\n", node.id);

                        return;
                    }
                }
                y++;
            }
        }
    }
}
void createprocesses(int **graph, int n, node_t *nodes)
{

    for (int i = 0; i < n; i++)
    {
        int pid = fork();
        if (pid == 0)
        {

            for (int y = 0; y < n; y++)
            {
                if (y != i)
                {
                    close(nodes[y].fd[0]);
                }
                if (graph[i][y] != 1)
                {
                    close(nodes[y].fd[1]);
                }
            }
            narg_t narg;
            narg.graph = graph;
            narg.node = nodes[i];
            NodeProcess(&narg);
            for (int y = 0; y < n; y++)
            {
                if (y == i)
                {
                    close(nodes[y].fd[0]);
                }
                if (graph[i][y] == 1)
                {
                    close(nodes[y].fd[1]);
                }
            }

            freegraph(graph);
            free(nodes);
            exit(EXIT_SUCCESS);
        }
    }
}
void queen(node_t *nodes, int n, int start, int end, int fd)
{

    for (int i = 0; i < n; i++)
    {
        close(nodes[i].fd[0]);
        if (i != start)
        {
            close(nodes[i].fd[1]);
        }
    }
    ant_t ant;
    memset(&ant, 0, sizeof(ant));
    ant.dest = end;
    while (!stop_work && write(nodes[start].fd[1], &ant, sizeof(ant)) > 0)
    {
        sleep(1);
        ant.id++;
        ant_t survivor;
        char buff[500];
        if (read(fd, &survivor, sizeof(survivor)) == sizeof(survivor))
        {
            int r = sprintf(buff, "Ant %d path: ", survivor.id);
            for (int i = 0; i < survivor.path_len; i++)
            {
                r += sprintf(buff + r, " %d", survivor.path[i]);
            }
            puts(buff);
        }
    }
    unlink(FIFO_NAME);
    close(nodes[start].fd[1]);
    kill(0, SIGINT);
}
void nukehandler(int sig)
{
    if (!stop_work)
        kill(0, SIGINT);
    stop_work = 1;
}
int main(int argc, char *argv[])
{

    set_handler(nukehandler, SIGINT);
    set_handler(SIG_IGN, SIGPIPE);
    unlink(FIFO_NAME);
    mkfifo(FIFO_NAME, 0666);
    int fd = open(FIFO_NAME, O_RDONLY | O_NONBLOCK);
    if (argc != 4)
    {
        usage(argc, argv);
    }
    int start = atoi(argv[2]);
    int dest = atoi(argv[3]);
    int **graph = malloc(MAX_GRAPH_NODES * sizeof(int *));
    if (graph == NULL)
    {
        ERR("malloc");
    }

    for (int i = 0; i < MAX_GRAPH_NODES; i++)
    {
        graph[i] = malloc(MAX_GRAPH_NODES * sizeof(int));
        if (graph[i] == NULL)
        {
            ERR("malloc");
        }
        memset(graph[i], 0, MAX_GRAPH_NODES * sizeof(int));
    }

    int node_num;
    loadGraph(graph, argv[1], &node_num);
    node_t *nodes = malloc(node_num * sizeof(node_t));

    createNodes(graph, nodes, node_num);
    createprocesses(graph, node_num, nodes);
    queen(nodes, node_num, start, dest, fd);
    free(nodes);
    freegraph(graph);

    while (wait(NULL) > 0)
        ;
}