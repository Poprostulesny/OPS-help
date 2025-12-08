#POSIx
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
#include<sys/wait.h>
#define RAND_MAX 100

#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))


volatile sig_atomic_t lastsignal=0;
volatile sig_atomic_t lastsender=0;
int checked_strtol(char* arg){
    errno = 0;
    int val = strtol(arg, NULL,10);
    if(errno!=0&&val==0){
        ERR("Wrong input argument, has to be of type int");
    }
    return val;
}
void sigusr1_handler(int sigNo, siginfo_t *info ,  void *ucontext){
    lastsignal=SIGUSR1;
    if(info) lastsender = info->si_pid;
}
void sigusr2_handler(int sigNo){
    lastsignal=SIGUSR2;
}
void sethandler(void (*f)(int, siginfo_t *, void*), int sigNo){
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    act.sa_flags=SA_SIGINFO;
    if(-1 == sigaction(sigNo, &act, NULL)){
        ERR("sigaction");
    }
 
}
void student_work(int no, int p, int t, int prob, sigset_t oldmask){
    srand(getpid()%15*time(NULL));
     struct timespec t1;
     t1.tv_nsec=100;
     t1.tv_sec=0;
     int isfailure;
     int issue_count =0;
  
    printf("Student[%d,%d] has started doing task!", no, getpid());
    for(int i=0;i<p;i++){
        printf("Student[%d,%d] is starting doing part %d of %d", no, getpid(), i+1, p);
        for(int y= 0;y<t;y++){
        nanosleep(&t,NULL);
        isfailure= rand();
        if(isfailure<prob){
            issue_count++;
            
            printf("Student[%d,%d] has issue (%d) doing task!", no, getpid(), issue_count);
          
        }   
        printf("Student[%d,%d] has finished doing part %d of %d", no, getpid(), i+1, p);
        kill(getppid(), SIGUSR1);
        sigsuspend(&oldmask);
        }
        
    }
    
    exit(issue_count);

}
void teacher_work(int p, int t, int * prob, int prob_size, sigset_t oldmask){
    for(int i=0;i< prob_size;i++){
        pid_t p =0;
        if((p=fork())==0){
            student_work(i, p, t, prob[i], oldmask);
        }
    }
    while(1){
        sigsuspend(&oldmask);
        if(lastsignal==SIGUSR1){    
            //sending sigusr2 to the program which has sent the signal

        }
        lastsignal=0;
    }


}
int main(int argc, char*argv[]){
    if(argc<=3){
        fprintf(stderr, "Usage: %s <arg1> <arg2> <arg3>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    errno = 0;
    int p = checked_strtol(argv[1]);
    int t = checked_strtol(argv[2]);
    int cnt = 3;
    int *probabilities = malloc(sizeof(int)*(argc-3) );
    while(cnt<argc){
        probabilities[cnt-3]=checked_strtol(argv[cnt]);
        cnt++;
    }
    sethandler(sigusr1_handler, SIGUSR1);
      sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR2);
    sigprocmask(SIG_BLOCK,&mask, &oldmask );
    teacher_work(p, t, probabilities, argc-3, oldmask);
    sigprocmask(SIG_UNBLOCK, &mask,NULL);







}