#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <bits/sigaction.h>
#include <sys/wait.h>
// Na początku pliku, po #define ERR:
#define SIG_STUDENT SIGRTMIN
#define SIG_TEACHER (SIGRTMIN + 1)

#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

volatile sig_atomic_t lastsignal = 0;

volatile sig_atomic_t pending_students[100];
volatile sig_atomic_t pending_head = 0;
volatile sig_atomic_t pending_tail = 0;
int childcount = 0;
int *probabilities;
int *student_pid;
int *student_exit;
int alive_children;

int checked_strtol(char *arg)
{
    errno = 0;
    int val = strtol(arg, NULL, 10);
    if (errno != 0 && val == 0)
    {
        ERR("Wrong input argument, has to be of type int");
    }
    return val;
}
void SIG_STUDENT_handler(int sigNo, siginfo_t *info, void *ucontext)
{
    lastsignal = SIG_STUDENT;
    if (info)
    {
        pending_students[pending_tail] = info->si_pid;
        pending_tail = (pending_tail + 1) % 100;
    }
}
void SIG_TEACHER_handler(int sigNo, siginfo_t *info, void *ucontext)
{
    lastsignal = SIG_TEACHER;
}
void sigchld_handler(int sigNo, siginfo_t *info, void *ucontext)
{
    lastsignal = SIGCHLD;
}

void sethandler(void (*f)(int, siginfo_t *, void *), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_sigaction = f;
    act.sa_flags = SA_SIGINFO | SA_NODEFER | SA_RESTART;
    
    if (-1 == sigaction(sigNo, &act, NULL))
    {
        ERR("sigaction");
    }
}

void student_work(int no, int p, int t, int prob, sigset_t newmask)
{
    srand(getpid() ^ time(NULL));

    struct timespec t1;
    t1.tv_nsec = 100 * 1000 * 1000L;
    t1.tv_sec = 0;

    int isfailure;
    int issue_count = 0;
    
    sigset_t waitset;
    sigfillset(&waitset);
    sigdelset(&waitset, SIG_TEACHER);

    printf("Student[%d,%d] has started doing task!\n", no, getpid());
    for (int i = 0; i < p; i++)
    {
        printf("Student[%d,%d] is starting doing part %d of %d\n", no, getpid(), i + 1, p);
        for (int y = 0; y < t; y++)
        {
            t1.tv_nsec = 100 * 1000 * 1000L;
            t1.tv_sec = 0;
            nanosleep(&t1, NULL);
            isfailure = rand() % 100 + 1;

            if (isfailure <= prob)
            {
                issue_count++;

                printf("Student[%d,%d] has issue (%d) doing task!\n", no, getpid(), issue_count);
                t1.tv_nsec = 50 * 1000 * 1000L;
                nanosleep(&t1, NULL);
            }
        }

        printf("Student[%d,%d] has finished doing part %d of %d\n", no, getpid(), i + 1, p);
        lastsignal = 0;
        kill(getppid(), SIG_STUDENT);

        while (lastsignal != SIG_TEACHER)
        {
            sigsuspend(&waitset);
        }
    }

    exit(issue_count);
}
int count_children()
{
    int cnt = 0;
    for (int i = 0; i < childcount; i++)
    {
        if (student_exit[i] == -10)
        {
            cnt++;
        }
    }
    return cnt;
}

void teacher_work(int p, int t, int *prob, int prob_size, int *student_pid, sigset_t oldmask)
{
    for (int i = 0; i < prob_size; i++)
    {
        pid_t child = 0;
        if ((child = fork()) == 0)
        {
            student_work(i + 1, p, t, prob[i], oldmask);
        }
        else
        {
            student_pid[i] = child;
        }
    }
    while (alive_children > 0)
    {

        sigsuspend(&oldmask);

        while (pending_head != pending_tail)
        {
            kill(pending_students[pending_head], SIG_TEACHER);
            printf("Teacher has accepted solution of student [%d], alive children: %d\n", pending_students[pending_head], alive_children);
            pending_head = (pending_head + 1) % 100;
        }

        pid_t pid;
        int status;
        
        while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
        {
            for (int i = 0; i < childcount; i++)
            {
                if (student_pid[i] == pid)
                {
                    student_exit[i] = WEXITSTATUS(status);
                    break;
                }
            }
        }

        alive_children = count_children();
        lastsignal = 0;
    }

    while (wait(NULL) > 0)
        ;

    printf("No. | Student ID | Issue Count\n");
    int total = 0;

    for (int i = 0; i < prob_size; i++)
    {
        printf("%3d | %10d | %11d\n", i + 1, student_pid[i], student_exit[i]);
        total += student_exit[i];
    }

    printf("Total issues: %d\n", total);
}
int main(int argc, char *argv[])
{
    if (argc <= 3)
    {
        fprintf(stderr, "Usage: %s <arg1> <arg2> <arg3>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    errno = 0;
    int p = checked_strtol(argv[1]);
    int t = checked_strtol(argv[2]);
    int cnt = 3;

    probabilities = malloc(sizeof(int) * (argc - 3));
    student_pid = malloc(sizeof(int) * (argc - 3));
    student_exit = malloc(sizeof(int) * (argc - 3));
    childcount = argc - 3;
    alive_children = childcount;

    printf("%d\n", getpid());

    for (int i = 0; i < childcount; i++)
    {
        student_exit[i] = -10;
    }

    while (cnt < argc)
    {
        probabilities[cnt - 3] = checked_strtol(argv[cnt]);
        cnt++;
    }
    sethandler(SIG_STUDENT_handler, SIG_STUDENT);
    sethandler(SIG_TEACHER_handler, SIG_TEACHER);
    sethandler(sigchld_handler, SIGCHLD);

    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIG_TEACHER);
    sigaddset(&mask, SIGCHLD);
    sigaddset(&mask, SIG_STUDENT);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);

    teacher_work(p, t, probabilities, argc - 3, student_pid, oldmask);

    sigprocmask(SIG_UNBLOCK, &mask, NULL);
    free(probabilities);
    free(student_exit);
    free(student_pid);
}