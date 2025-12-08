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


#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))


volatile sig_atomic_t lastsignal=0;
int numSigUsr1 =0;
void sigusr1_handler(int signum)
{
    lastsignal=signum;
}
void sigchld_handler(int sigNo){
    pid_t pid;

    while (1)
    {
        pid = waitpid(0, NULL, WNOHANG);

        if (pid == 0)
            return;

        if (pid <= 0)
        {
            if (errno == ECHILD)
                return;
            ERR("waitpid");
        }
    }
}
void sethandler(void (*f)(int), int sigNo){
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if(-1 == sigaction(sigNo, &act, NULL)){
        ERR("sigaction");
    }
 
}
void sigusr2_handler(int signum){
    lastsignal=signum;
}

void child_work()
{   
    lastsignal=0;
    sethandler(sigusr2_handler, SIGUSR2);
    srand(time(NULL)*getpid());
    int s = rand()%2000+500;
    printf("I am a child my pid is: %d, i will be waiting for: %dms\n",getpid(), s);
    for(int i=0;i<30;i++){
        struct timespec t1;
        t1.tv_sec = s / 1000;
        t1.tv_nsec = (s % 1000) * 1000000;
        
        kill(getppid(), SIGUSR1);
        printf("I am a child my pid is: %d, I have sent %dth SIGUSR1 to my dad. Now taking another nap for: %dms\n",getpid(), i, s);
        for(int y=0;y<=i;y++){
            printf("*");
        }
        printf("\n");
        nanosleep(&t1, NULL);
        
        if(lastsignal==SIGUSR2){
            printf("I was child %d I sent %d signals, I am getting KILLED\nNOW I AM GONNA KILL THE CHILDREN\n", getpid(), i);
            exit(EXIT_SUCCESS);
        }
    }
}

void main_work(int n, sigset_t oldmask)
{
    pid_t group =0;
  for(int i=0;i<n;i++){
    pid_t p;
    if((p=fork())<0){
        ERR("fork failed");
    }
    if(p==0){
        child_work();
        exit(EXIT_SUCCESS);
    }
    if(i==0){
        group = p;
        
    }
    setpgid(p, group);
  }
  
  while(numSigUsr1<100){
    lastsignal=0;
    while(lastsignal!= SIGUSR1){
         sigsuspend(&oldmask);
    }
    
    numSigUsr1++;
    printf("I AM PARENT I GOT THE %dnth signal\n", numSigUsr1);
  }
  kill(-group, SIGUSR2);
  

}

int main(int argc, char *argv[])
{

    if (argc != 2)
    {
        exit(EXIT_FAILURE);
    }
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    
    sigprocmask(SIG_BLOCK,&mask, &oldmask );
    sethandler(sigusr1_handler, SIGUSR1);
    sethandler(sigchld_handler, SIGCHLD);
    int n = atoi(argv[1]);
    main_work(n, oldmask);
    sigprocmask(SIG_UNBLOCK, &mask, NULL);
    while(wait(NULL)>0);
    printf("lala");
    exit(EXIT_SUCCESS);
}   