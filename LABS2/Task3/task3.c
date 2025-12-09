#define _GNU_SOURCE
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
#include <fcntl.h>

/* Write a program simulating a computational cluster.

The parent process creates N children, where N is the only command-line argument accepted by the program. Each child is assigned a number between 0 and N-1.

The parent instructs the first child to do work by sending SIGUSR1 to it. After receiving the signal, the child performs its work in a loop: it sleeps for a random amount of time from the range 100-200ms, and adds 1 to a local counter and prints {PID}: {counter}\n to stdout.

If the parent receives SIGUSR1, it sents SIGUSR2 to the currently working child and SIGUSR1 to the next child, i.e. the one with its index larger by one (for the last child, the next one should be the child with index 0). The child which receives SIGUSR2 stops its loop and waits for SIGUSR1 to resume its work. In short, SIGUSR1 instructs a child to begin its work, and SIGUSR2 to stop it.

After receiving SIGINT, the parent passes on this signal to all children. After receiving SIGINT, the children save (using low-level file IO) their counter to {PID}.txt and end their work. The parent awaits termination of all of its children and then exits.
 */
#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

volatile sig_atomic_t last_signal = 0;

void sethandler(void (*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = f;
    sigemptyset(&act.sa_mask);
    if (-1 == sigaction(sigNo, &act, NULL))
    {
        ERR("sigaction");
    }
}
void sigint_handler(int signo)
{
    last_signal = signo;
}
void sigusr1_handler(int signo)
{
    last_signal = signo;
}
void sigusr2_handler(int signo)
{
    last_signal = signo;
}
void sigchld_handler(int signo)
{
    last_signal = signo;
}
void write_child(pid_t pid, int cnt){
    char filename[1000];
    snprintf(filename, 1000, "out/%d.txt", getpid());
    int fd = open(filename, O_WRONLY | O_CREAT,0644);
    if(fd==-1){
        ERR("open");
    }
    char content[32];
    int len = snprintf(content, 32, "%d",cnt );
    write(fd, content, len);
    close(fd);

    exit(EXIT_SUCCESS);


}
void childwork(int n)
{
    int counter = 0;
    srand(getpid() ^ time(NULL));
    struct timespec t;
    sigset_t e;
    sigemptyset(&e);
    sigprocmask(SIG_SETMASK,&e,NULL);
    sigset_t usr1;
    sigfillset(&usr1);
    sigdelset(&usr1, SIGUSR1);
    sigdelset(&usr1, SIGUSR2);
    sigdelset(&usr1, SIGINT);

    while (1)
    {
     
        sigsuspend(&usr1);

        

        while (last_signal!=SIGUSR2)
        {
            if (last_signal == SIGINT)
            {
                write_child(getpid(), counter);
            }

            t.tv_nsec = 1000000 * (rand() % 100 + 100);
            t.tv_sec = 0;
            printf("{%d}:%d\n", getpid(), counter);
            nanosleep(&t, NULL);
            counter++;
            
            
        }
        if(last_signal==SIGINT){
            write_child(getpid(), counter);
        }
        last_signal=0;
    }

    
}
void mainwork(int n)
{       
    printf("parent pid: %d\n",getpid());
    int worker = 0;
    int *pid_worker = malloc(n * sizeof(int));
    for (int i = 0; i < n; i++)
    {
        pid_t child;
        if ((child = fork()) == 0)
        {
            childwork(i);
        }
        else if(child<0){
            ERR("fork");
        }
        pid_worker[i] = child;
    }
    kill(pid_worker[0], SIGUSR1);

    sigset_t m;
    sigfillset(&m);
    sigdelset(&m, SIGUSR1);
    sigdelset(&m, SIGINT);
    last_signal=0;
    while (last_signal != SIGINT)
    {   
        sigsuspend(&m);
        
        if(last_signal == SIGUSR1){
            kill(pid_worker[worker], SIGUSR2);

            worker=(worker+1)%n;
            kill(pid_worker[worker], SIGUSR1);
            last_signal=0;
        }     
    
    }
    
    kill(0, SIGINT);
    while(wait(NULL)>0);
    free(pid_worker);
    exit(EXIT_SUCCESS);

}
int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        ERR("too lil args");
    }
    int n = atoi(argv[1]);
    sethandler(sigint_handler,SIGINT);
    sethandler(sigusr1_handler, SIGUSR1);
    sethandler(sigusr2_handler,SIGUSR2);
    
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGUSR1);
    sigaddset(&mask, SIGUSR2);
    sigprocmask(SIG_BLOCK,&mask, &oldmask);
    mainwork(n);

    sigprocmask(SIG_UNBLOCK, &mask, NULL);
    exit(EXIT_SUCCESS);
}