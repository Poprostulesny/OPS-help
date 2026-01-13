#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#include<pthread.h>
#include<stdio.h>
#include<stdlib.h>
#include<semaphore.h>
#include<time.h>
#include<stddef.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include<time.h>
#include<string.h>
#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

typedef struct arg{
    int *arr;
    pthread_mutex_t *mut;
    int  n;
    int p;
}arg;

volatile sig_atomic_t sig1=0;
volatile sig_atomic_t sig2=0;
volatile sig_atomic_t sigint=0;
void sethandler(void (*handler)(int), int sig){
    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_handler = handler;
    if (sigaction(sig, &sa, NULL) == -1)
       ERR("sigaction");
}

void sigusr1handler(int sig){
    sig1=1;
}
    
void sigusr2handler(int sig){
    sig2=1;
}
void siginthandler(int sig){
    sigint=1;
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
// n - tasks p - threads
sem_t quitter;
void * thread_work(void * args){
    arg * ar = (arg*)args;
    int  p =ar->p;
    int n = ar->n;
    int *arr = ar->arr;
    pthread_mutex_t *mut = ar->mut;
    sem_wait(&quitter);
    int seed = time(NULL);
    int a = rand_r(&seed)%n;
    int b = rand_r(&seed)%n;
    if(a==b){
        a=(a+1)%n;    
    }
    if(a>b){
        int tmp;
        tmp=a;
        a=b;
        b=tmp;
    }
    int ap = a;
    int bp = b;
    while(ap!=a+(b-a-1)/2){
        pthread_mutex_lock(&mut[ap]);
        pthread_mutex_lock(&mut[bp]);
        int tmp;
        tmp=arr[ap];
        arr[ap]=arr[bp];
        arr[bp]=tmp;
        pthread_mutex_unlock(&mut[ap]);
        pthread_mutex_unlock(&mut[bp]);
        
        ap++;
        bp--;
        msleep(5);
    }
    sem_post(&quitter);
    puts("I, a worker thread, have finished my mission");
    return NULL;
}

void * thread_printer(void *args){
    arg *ar = (arg*)args;
    int n = ar->n;
    pthread_mutex_t *mut = ar->mut;
    int * arr = ar->arr;
    for(int i=0;i<n;i++){
        pthread_mutex_lock(&mut[i]);
    }
    puts("Array");
    for(int i=0;i<n;i++){
        printf("%d ", arr[i]);
    }
    puts("");

    for(int i=0;i<n;i++){
        pthread_mutex_unlock(&mut[i]);
    }

    
    return NULL;

}


void signalhandler(sigset_t mask, int arr[], int n, int p,  pthread_mutex_t *mut){
    pthread_t tid;
    int signo;
    pthread_attr_t attr;                    
    if(pthread_attr_init(&attr)){
        ERR("pthread_attr_init");
    }

    if(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED)){
        ERR("pthread_attr_setdetachstate");
    }
    arg argument;
    argument.arr=arr;
    argument.mut= mut;
    argument.n=n;
    argument.p=p;
    while(1){
        if(sigsuspend(&mask)==-1&&errno!=EINTR){
            ERR("sigwait");
        }
        if(sig1){
            if(sem_trywait(&quitter)==-1){
                if (errno==EAGAIN){
                    puts("All threads busy, aborting request");
                }
                
            }
            else{
                    sem_post(&quitter);                   
                    pthread_create(&tid, &attr, thread_work, &argument);
                }
            sig1=0;

        }
        if(sig2){
           
            pthread_create(&tid, &attr, thread_printer,&argument);   
            sig2=0;  
                 
        }
        if(sigint){
            pthread_attr_destroy(&attr);
            return;
        }
    }



}

int main(int argc, char** argv){
    if(argc<3){
        exit(EXIT_FAILURE);
    }
    int n = atoi(argv[1]);
    int p = atoi(argv[2]);
    if(n<8||n>256||p<1||p>16){
        exit(EXIT_FAILURE);
    }
    if(sem_init(&quitter, 0, p)==-1){
        ERR("sem_init failed\n");
    }
    sethandler(sigusr1handler, SIGUSR1);
    sethandler(sigusr2handler, SIGUSR2);
    srand(time(NULL));
    printf("PID: %d\n", getpid());
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGUSR2);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);
    int *arr = (int*)malloc(n*sizeof(int));
    pthread_mutex_t *mut = (pthread_mutex_t*)malloc(n*sizeof(pthread_mutex_t));
    for(int i=0;i<n;i++){
        if(pthread_mutex_init(&mut[i], NULL)){
            ERR("pthread mutex inti");
        }
    }
    if(arr==NULL){
        ERR("malloc fail");
    }
    for(int i = 0; i<n; i++){
        arr[i]=i;
    }

    signalhandler(oldmask, arr,n,p, mut );

    for(int i=0;i<p;i++){
        sem_wait(&quitter);
    }
    free(mut);
    free(arr);
    exit(EXIT_SUCCESS);


}