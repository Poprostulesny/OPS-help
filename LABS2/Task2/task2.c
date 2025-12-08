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
#define RAND_MAX 100

#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

volatile sig_atomic_t lastsignal = 0;
volatile sig_atomic_t lastsender = 0;
volatile sig_atomic_t lastreturn = 0;

int childcount=0;
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
void sigusr1_handler(int sigNo, siginfo_t *info, void *ucontext)
{
    lastsignal = SIGUSR1;
    if (info)
        lastsender = info->si_pid;
}
void sigusr2_handler(int sigNo, siginfo_t *info, void *ucontext)
{
    lastsignal = SIGUSR2;
    if (info)
        lastsender = info->si_pid;
}
void sigchld_handler(int sigNo, siginfo_t *info, void *ucontext)
{
    lastsignal = SIGCHLD;
    if (info)
        lastsender = info->si_pid;
    pid_t pid;
    alive_children--;
    while (1)
    {   int status;
        pid = waitpid(0, &status, WNOHANG);
        
        if (pid == 0)
            return;

        if (pid <= 0)
        {
            if (errno == ECHILD)
                return; 
            return;
        }
        else{
            for(int i=0;i<childcount;i++){
                if(student_pid[i]==info->si_pid){
                    student_exit[i]=status;
                    return;
                }
            }
        }
    }
}

void sethandler(void (*f)(int, siginfo_t *, void *), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_sigaction = f;
    act.sa_flags = SA_SIGINFO;
    if (-1 == sigaction(sigNo, &act, NULL))
    {
        ERR("sigaction");
    }
}

void student_work(int no, int p, int t, int prob, sigset_t newmask)
{
    srand(getpid() % 15 * time(NULL));
    struct timespec t1;
    t1.tv_nsec = 100;
    t1.tv_sec = 0;
    int isfailure;
    int issue_count = 0;

    printf("Student[%d,%d] has started doing task!\n", no, getpid());
    for (int i = 0; i < p; i++)
    {
        printf("Student[%d,%d] is starting doing part %d of %d\n", no, getpid(), i + 1, p);
        for (int y = 0; y < t; y++)
        {
            nanosleep(&t1, NULL);
            isfailure = rand();
            if (isfailure < prob)
            {
                issue_count++;

                printf("Student[%d,%d] has issue (%d) doing task!\n", no, getpid(), issue_count);
            }

            printf("Student[%d,%d] has finished doing part %d of %d\n", no, getpid(), i + 1, p);
            lastsignal = 0;
            kill(getppid(), SIGUSR1);

            while (lastsignal != SIGUSR2)
            {
                sigsuspend(&newmask);
                
            }
        }
    }

    exit(issue_count);
}
int count_children(){
    int cnt=0;
    for(int i=0;i<childcount;i++){
        if(student_exit[i]==-10){
            cnt++;
        }   
    }
    return cnt;
}
void teacher_work(int p, int t, int *prob, int prob_size, int * student_pid , sigset_t oldmask)
{
    for (int i = 0; i < prob_size; i++)
    {
        pid_t p = 0;
        if ((p = fork()) == 0)
        {
            student_work(i, p, t, prob[i], oldmask);
        }
        else{
            student_pid[i]=p;
        }
    }
    while (alive_children>0)
    {

        sigsuspend(&oldmask);
        alive_children= count_children();
        if(lastsignal == SIGUSR1){
            kill(lastsender, SIGUSR2);
        }
        
        lastsender = 0;
        lastsignal = 0;

        printf("Teacher has accepted solution of student [%d]\n", lastsender);
    }

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
    student_pid = malloc(sizeof(int)*(argc-3));
    student_exit = malloc(sizeof(int)*(argc-3));
    childcount= argc-3;
    alive_children=childcount;
    
    for(int i=0;i<childcount;i++){
        student_exit[i]=-10;
    }
  
    while (cnt < argc)
    {
        probabilities[cnt - 3] = checked_strtol(argv[cnt]);
        cnt++;
    }
    sethandler(sigusr1_handler, SIGUSR1);
    sethandler(sigusr2_handler, SIGUSR2);
    sethandler(sigchld_handler, SIGCHLD);

    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR2);
    sigaddset(&mask, SIGCHLD);
    sigaddset(&mask, SIGUSR1);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);

    teacher_work(p, t, probabilities, argc - 3, student_pid, mask);

    sigprocmask(SIG_UNBLOCK, &mask, NULL);  
    free(probabilities);
    free(student_exit);
    free(student_pid);
}