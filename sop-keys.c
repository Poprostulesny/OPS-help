#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <semaphore.h>

#define KEYBOARD_CAP 10
#define SHARED_MEM_NAME "/memory"
#define MIN_STUDENTS KEYBOARD_CAP
#define MAX_STUDENTS 20
#define MIN_KEYBOARDS 1
#define MAX_KEYBOARDS 5
#define MIN_KEYS 5
#define MAX_KEYS KEYBOARD_CAP

#define ERR(source)                                     \
    do                                                  \
    {                                                   \
        fprintf(stderr, "%s:%d\n", __FILE__, __LINE__); \
        perror(source);                                 \
        kill(0, SIGKILL);                               \
        exit(EXIT_FAILURE);                             \
    } while (0)

void usage(char *program_name)
{
    fprintf(stderr, "Usage: \n");
    fprintf(stderr, "\t%s n m k\n", program_name);
    fprintf(stderr, "\t  n - number of students, %d <= n <= %d\n", MIN_STUDENTS, MAX_STUDENTS);
    fprintf(stderr, "\t  m - number of keyboards, %d <= m <= %d\n", MIN_KEYBOARDS, MAX_KEYBOARDS);
    fprintf(stderr, "\t  k - number of keys in a keyboard, %d <= k <= %d\n", MIN_KEYS, MAX_KEYS);
    exit(EXIT_FAILURE);
}

void ms_sleep(unsigned int milli)
{
    time_t sec = (int)(milli / 1000);
    milli = milli - (sec * 1000);
    struct timespec ts = {0};
    ts.tv_sec = sec;
    ts.tv_nsec = milli * 1000000L;
    if (nanosleep(&ts, &ts))
        ERR("nanosleep");
}

void print_keyboards_state(double *keyboards, int m, int k)
{
    for (int i = 0; i < m; ++i)
    {
        printf("Klawiatura nr %d:\n", i + 1);
        for (int j = 0; j < k; ++j)
            printf("  %e", keyboards[i * k + j]);
        printf("\n\n");
    }
}
typedef struct barrierstruct
{
    pthread_barrier_t barrier;
} k_t;
typedef struct sharedkeyboards
{
    double keys[MAX_KEYS * MAX_KEYBOARDS];
    pthread_mutex_t mutexes[MAX_KEYS * MAX_KEYBOARDS];
    int flag;
    pthread_mutex_t fmut;
} sh_k;
int safelock(pthread_mutex_t *mut, int id, pthread_mutex_t *flagmut, int *flag)
{
    if (pthread_mutex_lock(mut) == EOWNERDEAD)
    {
        printf("Student %d: someone is lying here, help !!!", id);
        if (pthread_mutex_lock(flagmut) == EOWNERDEAD)
        {
            pthread_mutex_consistent(flagmut);
        }
        *flag = 1;
        pthread_mutex_unlock(flagmut);
        if (pthread_mutex_consistent(mut) == -1)
        {
            ERR("pthread mutex");
        }
        return 1;
    }
    return 0;
}
void childwork(int id, int m, int fd, pthread_barrier_t *barrier, int k)
{
    sem_t *sems[MAX_KEYBOARDS];
    char semname[256];
    srand(time(NULL) ^ getpid());

    pthread_barrier_wait(barrier);
    for (int i = 0; i < m; i++)
    {
        snprintf(semname, sizeof(semname), "/sop-sem-%d", i);
        sems[i] = sem_open(semname, O_CREAT, 0666, KEYBOARD_CAP);
    }
    int fds = shm_open(SHARED_MEM_NAME, O_CREAT | O_RDWR, 0666);
    if (fds == -1)
        ERR("shm");

    if (ftruncate(fds, sizeof(sh_k)) == -1)
        ERR("ftrucn");

    sh_k *keys = mmap(NULL, sizeof(sh_k), PROT_READ | PROT_WRITE, MAP_SHARED, fds, 0);
    if (keys == MAP_FAILED)
        ERR("mmap");

    for (int i = 0; i < 10; i++)
    {

        int c = rand() % m;
        int key = rand() % k;
        int kill = rand() % 100;
        sem_wait(sems[c]);
        printf("Student %d[%d]: cleaning keyboard %d, key %d\n", id, getpid(), c, key);

        safelock(&keys->mutexes[c * k + key], id, &keys->fmut, &keys->flag);
        msync(keys, sizeof(sh_k), O_SYNC);
        if (kill == 0)
        {
            printf("Student %d[%d]: I have no more strength!\n", id, getpid());
            sem_post(sems[c]);
            abort();
        }
        keys->keys[c * k + key] /= 3.0;

        ms_sleep(300);
        msync(keys, sizeof(sh_k), O_SYNC);
        pthread_mutex_unlock(&keys->mutexes[c * k + key]);
        sem_post(sems[c]);
        if (pthread_mutex_lock(&keys->fmut) == EOWNERDEAD)
        {
            pthread_mutex_consistent(&keys->fmut);
        }
        if (keys->flag == 1)
        {
            puts("AAAAAAA!!!!!");
            pthread_mutex_unlock(&keys->fmut);
            break;
        }
        pthread_mutex_unlock(&keys->fmut);
    }
    munmap(keys, sizeof(sh_k));
    close(fds);
}
void init_proc(int n, int m, int k, int fd, pthread_barrier_t *barrier)
{
    char semname[256];

    for (int i = 0; i < n; i++)
    {
        snprintf(semname, sizeof(semname), "/sop-sem-%d", i);
        sem_unlink(semname);
    }

    for (int i = 0; i < n; i++)
    {
        int pid = fork();
        if (pid == 0)
        {
            childwork(i, m, fd, barrier, k);
            exit(EXIT_SUCCESS);
        }
    }
}

int main(int argc, char **argv)
{
    if (argc != 4)
    {
        usage(argv[0]);
    }
    int n = atoi(argv[1]);
    int m = atoi(argv[2]);
    int k = atoi(argv[3]);

    if (n > MAX_STUDENTS /*|| n < MIN_STUDENTS */ || m < MIN_KEYBOARDS || m > MAX_KEYBOARDS || k < MIN_KEYS || k > MAX_KEYS)
    {
        usage(argv[0]);
    }
    // shared memory
    int fd = open("bar", O_CREAT | O_RDWR | O_TRUNC, 0666);

    if (fd == -1)
    {
        ERR("open");
    }
    if (ftruncate(fd, sizeof(k_t)) == -1)
    {
        ERR("ftruncate");
    }

    k_t *keyboard = mmap(NULL, sizeof(k_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (keyboard == MAP_FAILED)
    {
        ERR("MMAp");
    }
    // barrier init
    pthread_barrierattr_t atr;
    pthread_barrierattr_init(&atr);
    pthread_barrierattr_setpshared(&atr, PTHREAD_PROCESS_SHARED);
    pthread_barrier_init(&keyboard->barrier, &atr, n + 1);
    // core
    init_proc(n, m, k, fd, &keyboard->barrier);
    //      shared mem
    int fds = shm_open(SHARED_MEM_NAME, O_CREAT | O_RDWR | O_TRUNC, 0666);
    if (fds == -1)
        ERR("shm");

    if (ftruncate(fds, sizeof(sh_k)) == -1)
        ERR("ftrucn");

    sh_k *keys = mmap(NULL, sizeof(sh_k), PROT_READ | PROT_WRITE, MAP_SHARED, fds, 0);
    if (keys == MAP_FAILED)
        ERR("mmap");

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);
    for (int i = 0; i < m * k; i++)
    {
        pthread_mutex_init(&keys->mutexes[i], &attr);
        keys->keys[i] = 1;
    }
    pthread_mutex_init(&keys->fmut, &attr);
    keys->flag = 0;

    ms_sleep(500);
    puts("CLEANING STARTS");
    pthread_barrier_wait(&keyboard->barrier);

    // cleanup
    while (wait(NULL) > 0)
        ;
    print_keyboards_state(keys->keys, m, k);
    pthread_barrier_destroy(&keyboard->barrier);
    munmap(keyboard, sizeof(k_t));
    munmap(keys, sizeof(sh_k));
    unlink(SHARED_MEM_NAME);
    close(fd);
    close(fds);
}