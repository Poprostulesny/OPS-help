#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#include<pthread.h>
#include<stdio.h>
#include<signal.h>
#include<unistd.h>
#include<time.h>
#include<stdlib.h>
#include<string.h>
#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))
typedef struct thread_arg{
    int n;
    int *arr;
    int id;
    pthread_mutex_t *mut;
    pthread_mutex_t *cnt_mut;
    int *cnt;
    int *res;
    volatile sig_atomic_t *quit_flag;
    pthread_mutex_t *flag_mut;

}thread_arg;

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
int move_dog(int pos_start, int pos_end, int * arr, pthread_mutex_t *mut, int id){
    if(pos_start==pos_end){
        return 1;
    }
      pthread_mutex_lock(&mut[pos_end]);
    if(arr[pos_end]!=0){
        printf("\x1b[31m%s\x1b[0m\n", "waf waf waf");
        pthread_mutex_unlock(&mut[pos_end]);
        return 0;
    }
    arr[pos_end]+=id;
    pthread_mutex_unlock(&mut[pos_end]);
    if(pos_start>=0){
        pthread_mutex_lock(&mut[pos_start]);
        arr[pos_start]-=id;
        pthread_mutex_unlock(&mut[pos_start]);
    }
    
  return 1;

}
void * racing_dog( void * arg){
    thread_arg *args = (thread_arg*)arg;
        int pos=-1;
        int sleep;
        int seed = time(NULL)^pthread_self();
        int move;
        int dir=1;
        int from = -1;
        
        while(1){
            pthread_mutex_lock(args->flag_mut);
            if(*args->quit_flag == 1){
                pthread_mutex_unlock(args->flag_mut);
                return NULL;
            }
            pthread_mutex_unlock(args->flag_mut);
            sleep = rand_r(&seed)%1320+200;
            msleep(sleep);
            move = rand_r(&seed)%5+1;
            if(pos+dir*move==args->n-1){
                //finishes the race;
                pthread_mutex_lock(args->cnt_mut);
                *args->res=(*args->cnt)++;
                pthread_mutex_unlock(args->cnt_mut);

                pthread_mutex_lock(&args->mut[pos]);
                args->arr[pos]-=args->id;
                pthread_mutex_unlock(&args->mut[pos]);
                printf("Dog %d, finished the race at position %d\n", args->id, *args->res);
                return NULL;
            }
            from=pos;
            if(pos+dir*move>args->n-1){
                if(move_dog(pos,args->n-1, args->arr, args->mut, args->id)==1){
                    pos= args->n-1;
                    dir*=-1;   
                }
                          
            }

             else  if(pos+dir*move<=0){
                if(move_dog(pos, 0, args->arr, args->mut, args->id)==1){
                    pos=0;
                    dir*=-1;
                }
                
            }
            else{
                if(move_dog(pos, pos+dir*move, args->arr, args->mut, args->id)==1){
                    pos+=dir*move;
                }
            
            }
            printf("Moved dog %d from %d to %d\n", args->id, from, pos);

        }


}

int main(int argc, char**argv){
    if(argc!=3){
        printf("Usage: n m\n");
        exit(EXIT_SUCCESS);
    }   

    int n = atoi(argv[1]);
    int m = atoi(argv[2]);
    if(n<=20){
        printf("n>20");
        exit(EXIT_SUCCESS);
    }
    if(m<=2){
        printf("m>2");
        exit(EXIT_SUCCESS);
    }

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    pthread_sigmask(SIG_BLOCK, &mask, NULL);
    int* arr = (int*)malloc(n*sizeof(int));
    pthread_mutex_t *mut = (pthread_mutex_t*)malloc(n*sizeof(pthread_mutex_t));
    pthread_mutex_t cnt_mut;
    pthread_mutex_t flag_mut;
    memset(arr,0,n*sizeof(int));
    thread_arg* args = (thread_arg*)malloc(m*sizeof(thread_arg));
    volatile sig_atomic_t quit_flag=0;
    if(mut==NULL||arr==NULL||args==NULL){
        ERR("malloc");
    }
    for(int i=0;i<n;i++){
       if( pthread_mutex_init(&mut[i], NULL)){
        ERR("pthread mutexx intnin");
       }
    }
    if( pthread_mutex_init(&cnt_mut, NULL)){
        ERR("pthread mutex init");
    }
     if( pthread_mutex_init(&flag_mut, NULL)){
        ERR("pthread mutex init");
    }
    int cnt = 1;

    for(int i =0;i<m;i++){
        args[i].n=n;
        args[i].arr=arr;
        args[i].cnt=&cnt;
        args[i].cnt_mut=&cnt_mut;
        args[i].mut=mut;
        args[i].res=malloc(sizeof(int));
        args[i].quit_flag=&quit_flag;
        args[i].flag_mut =&flag_mut;
        args[i].id=i+1;

    }
pthread_t* dogs = (pthread_t*)malloc(m*sizeof(pthread_t));
    for(int i=0;i<m;i++){
        if(pthread_create(&dogs[i], NULL,racing_dog, &args[i] )){
            ERR("pthread create");
        }
    }

    while(1){
        pthread_mutex_lock(&cnt_mut);
        if(cnt==m+1){
            pthread_mutex_unlock(&cnt_mut);
            break;
        }
        pthread_mutex_unlock(&cnt_mut);
        struct timespec ts;
        ts.tv_sec=1;
        ts.tv_nsec=0;
       if( sigtimedwait(&mask, NULL, &ts)==SIGINT){
        pthread_mutex_lock(&flag_mut);
        quit_flag=1;
        pthread_mutex_unlock(&flag_mut);
        break;
       }
       
        for(int i=0;i<n;i++){
            pthread_mutex_lock(&mut[i]);

        }
        printf("\x1b[2J");       // clear screen
        printf("\x1b[999B");     // move cursor down a lot (clamped to last line)
        printf("\r");    
        puts("RACE");
        for(int i=0;i<n;i++){
            printf("%d ", arr[i]);

        }
        puts("");
        
        for(int i=0;i<n;i++){
            pthread_mutex_unlock(&mut[i]);

        }

    }
    for(int i=0;i<m;i++){
        pthread_join(dogs[i], NULL);
    }
    int dog1=-1, dog2=-1, dog3=-1;
    for(int i=0;i<m;i++){
        if(*args[i].res==1){
            dog1=args[i].id;
        }
        else if(*args[i].res==2){
            dog2=args[i].id;
        }
        else if(*args[i].res==3){
            dog3=args[i].id;
        }
    }
    for(int i=0;i<m;i++){
        free(args[i].res);
    }
    if(dog1!=-1){
        printf("Dog 1: %d\n", dog1);
    }
    if(dog2!=-1){
        printf("Dog 2: %d\n", dog2);
    }
    if(dog3!=-1){
        printf("Dog 3: %d\n", dog3);
    }
    free(dogs);
    free(mut);
    free(args);
    free(arr);
}
