#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#include<pthread.h>
#include<stdio.h>
#include<signal.h>
#include<unistd.h>
#include<time.h>
#include<stdlib.h>
#include<string.h>
#include<errno.h>
#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))
typedef struct thread_arg{
    pthread_cond_t * manager_signal;
    pthread_cond_t * worker_signal;
    pthread_mutex_t *worker_signal_mutex;
    pthread_mutex_t *storage_mut;
    int n;
    int m;
    int worker_id;
    int * cnt;
    pthread_mutex_t *cnt_mut;
    int **storage;
    int *deleted;
    pthread_mutex_t *deleted_mut;
    pthread_mutex_t * quit;
    volatile sig_atomic_t *quit_Flag;


}thread_arg;
void * dostawca(void  * arg){
    thread_arg* args = (thread_arg*)arg;
    int ** storage=args->storage;
    int n = args->n;
    int m = args->m;
    pthread_mutex_t *storage_mut=args->storage_mut;
    pthread_mutex_t *cnt_mut=args->cnt_mut;
    pthread_cond_t *manager_signal=args->manager_signal;
    unsigned int seed = time(NULL)^pthread_self();
    int shelf;
    int rack;
      pthread_mutex_t * quit = args->quit;
    volatile sig_atomic_t * quit_flag= args->quit_Flag;
    while(1){
         pthread_mutex_lock(quit);
        if(*quit_flag==0){
            pthread_cond_broadcast(manager_signal);
            pthread_mutex_unlock(quit);
            break;
        }
        pthread_mutex_unlock(quit);
      

        sleep(1);
        int r = rand_r(&seed);
        shelf = r%m;
        r=rand_r(&seed);
        rack=r%n;
        pthread_mutex_lock(cnt_mut);
        
        if(*args->cnt >= n*m){
            printf("I a courrier, have no more space in the storage. I will wait for the manager.\n");
            pthread_cond_wait(manager_signal,cnt_mut);
        }
        pthread_mutex_unlock(cnt_mut);
        pthread_mutex_lock(&storage_mut[rack]);
        if(storage[rack][shelf]==0){
           
            pthread_mutex_lock(cnt_mut);
            storage[rack][shelf]=1;
            (*args->cnt)++;
            printf("I, a courier, have added and item to shelf %d and rack %d, count now is: %d\n", shelf, rack, *args->cnt);
            pthread_mutex_unlock(cnt_mut);
        }
        else{
            printf("I, a courier, have tried to add an item to shelf %d and rack %d, but it was already occupied\n", shelf, rack);
        }
        pthread_mutex_unlock(&storage_mut[rack]);

    }

    return NULL;

}
void * odbiorca(void * arg){
    thread_arg* args = (thread_arg*)arg;
    int ** storage=args->storage;
    int n = args->n;
    int m = args->m;
    pthread_mutex_t *storage_mut=args->storage_mut;
    pthread_cond_t *worker_signal=args->worker_signal;
     unsigned int seed = time(NULL)^pthread_self();
    int rack;
    int * deleted = args->deleted;
    pthread_mutex_t* deleted_mut = args->deleted_mut;
    int worker_id = args->worker_id;
      pthread_mutex_t * quit = args->quit;
    volatile sig_atomic_t * quit_flag= args->quit_Flag;
    while(1){
         pthread_mutex_lock(quit);
        if(*quit_flag==0){
            pthread_cond_signal(worker_signal);
            pthread_mutex_unlock(quit);
            break;

        }
        pthread_mutex_unlock(quit);
        sleep(3);
        rack = rand_r(&seed);
        rack %= n;
        pthread_mutex_lock(&storage_mut[rack]);
        int found = 0;
        for(int i=0;i<m;i++){
            if(storage[rack][i]!=0){
                found++;
            }
            storage[rack][i]=0;
        }
        pthread_mutex_unlock(&storage_mut[rack]);
        if(found!=0){
            printf("I, worker %d, have deleted %d items from shelf %d\n", worker_id, found, rack);
            pthread_mutex_lock(&deleted_mut[worker_id]);
            deleted[worker_id]=found;
            pthread_mutex_unlock(&deleted_mut[worker_id]);
            pthread_cond_signal(worker_signal);
        }
        else{
            printf("I, worker %d, haven't found anything on rack %d\n", worker_id,  rack);
        }
        
    }
    return NULL;
}

void * manago_func(void *arg){
    thread_arg* args = (thread_arg*)arg;
    int *cnt = args->cnt;
   
    pthread_mutex_t *cnt_mut=args->cnt_mut;
    pthread_mutex_t * worker_signal_mut= args->worker_signal_mutex;
    pthread_cond_t *worker_signal=args->worker_signal;
    pthread_cond_t *manager_signal= args->manager_signal;
    int * deleted = args->deleted;
    pthread_mutex_t* deleted_mut = args->deleted_mut;
    pthread_mutex_t * quit = args->quit;
    volatile sig_atomic_t * quit_flag= args->quit_Flag;

    while(1){   
        pthread_mutex_lock(quit);
        if(*quit_flag==0){
            pthread_cond_broadcast(manager_signal);
            pthread_mutex_unlock(quit);
            printf("I, the manager, am qutiing this job\n");
            break;
        }
        pthread_mutex_unlock(quit);
        pthread_mutex_lock(worker_signal_mut);
        pthread_cond_wait(worker_signal, worker_signal_mut);
        pthread_mutex_unlock(worker_signal_mut);
        pthread_mutex_lock(cnt_mut);
        for(int i=0;i<3;i++){
            pthread_mutex_lock(&deleted_mut[i]);
            if(deleted[i]!=0){
                printf("I, the manager, have found that worker %d has removed %d items from the storage\n", i, deleted[i]);
                (*cnt)-=deleted[i];
                deleted[i]=0;
            }
            pthread_mutex_unlock(&deleted_mut[i]);
             
        }
        pthread_mutex_unlock(cnt_mut);
        pthread_cond_broadcast(manager_signal);
    }
   return NULL;
}

int main(int argc, char** argv){
    if(argc!=3){
        exit(EXIT_FAILURE);
    }
    int n = atoi(argv[1]);//regaly
    int m = atoi(argv[2]);//polki

    int **arr = malloc(n*sizeof(int*));
    if(arr==NULL){
        ERR("malloc");
    }
    for(int i=0;i<n;i++){
        arr[i]=malloc(m*sizeof(int));
        memset(arr[i], 0, m*sizeof(int));
        if(arr[i]==NULL){
            ERR("malloc");
        }
    }
    
     sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    pthread_sigmask(SIG_BLOCK, &mask, NULL);
    
    pthread_t *dostawcy = malloc(3*sizeof(pthread_t));
    pthread_t *odbiorcy = malloc(2*sizeof(pthread_t));
    pthread_t *manago = malloc(sizeof(pthread_t));
    volatile sig_atomic_t quit_flag = 1;
    pthread_mutex_t *dostawcy_report = malloc(3*sizeof(pthread_mutex_t));
    pthread_mutex_t *storage_mut = malloc(n*sizeof(pthread_mutex_t));
        pthread_mutex_t worker_signal_mutex = PTHREAD_MUTEX_INITIALIZER;
    int * dostawcy_report_arr = malloc(3*sizeof(int));
    pthread_mutex_t storage_full = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t manager_flag = PTHREAD_COND_INITIALIZER;
    thread_arg *args_odbiorcy = malloc(sizeof(thread_arg)*2);
    pthread_cond_t worker_signal=PTHREAD_COND_INITIALIZER;
    int cnt=0;
    pthread_mutex_t cnt_mut=PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t quit = PTHREAD_MUTEX_INITIALIZER;
    if(dostawcy==NULL || odbiorcy==NULL || manago == NULL||dostawcy_report==NULL||dostawcy_report_arr==NULL||args_odbiorcy==NULL||storage_mut==NULL){
        ERR("malloc");
    }
    for(int i=0;i<n;i++){
        if(pthread_mutex_init(&storage_mut[i],NULL)!=0){ERR("mutex_init");}
    }
    for(int i=0;i<3;i++){
            if(pthread_mutex_init(&dostawcy_report[i],NULL)!=0){
                ERR("mutex_init");
            }
            dostawcy_report_arr[i]=0;
    }


    for(int i=0;i<2;i++){
         args_odbiorcy[i].manager_signal= &manager_flag;
         args_odbiorcy[i].worker_signal = &worker_signal;
         args_odbiorcy[i].storage_mut = storage_mut;
         args_odbiorcy[i].storage=arr;
         args_odbiorcy[i].n=n;
         args_odbiorcy[i].worker_signal_mutex=&worker_signal_mutex;
         args_odbiorcy[i].m=m;
         args_odbiorcy[i].worker_id=i;
         args_odbiorcy[i].cnt= &cnt;
         args_odbiorcy[i].cnt_mut=&cnt_mut;
         args_odbiorcy[i].deleted = dostawcy_report_arr;
        args_odbiorcy[i].deleted_mut = dostawcy_report;
        args_odbiorcy[i].quit_Flag = &quit_flag;
        args_odbiorcy[i].quit= &quit;

    }
    int err=0;
    err=err|pthread_create(manago,NULL,manago_func,(void*)&args_odbiorcy[0]);
    for(int i=0;i<3;i++){
        if(i<2){
            err=err|pthread_create(&odbiorcy[i],NULL,odbiorca, (void*)&args_odbiorcy[i]);
        }
        err=err|pthread_create(&dostawcy[i], NULL, dostawca, (void*)&args_odbiorcy[0]);

    }
    if(err!=0){
        ERR("pthread_create");
    }
   
    int sig=0;
    while(sig!=SIGINT){
        sigwait(&mask, &sig);
    }
    pthread_mutex_lock(&quit);
    quit_flag=0;
    pthread_mutex_unlock(&quit);

    pthread_join(*manago, NULL);
    for(int i=0;i<3;i++){
        if(i<2){
            pthread_join(odbiorcy[i],NULL);
        }
       pthread_join(dostawcy[i], NULL);

    }

    //Cleaning up
    for(int i=0;i<n;i++){
        free(arr[i]);
    }
    free(arr);
    free(dostawcy);
    free(manago);
    free(odbiorcy);
    free(args_odbiorcy);
    free(dostawcy_report);
    free(dostawcy_report_arr);
    free(storage_mut);
    pthread_mutex_destroy(&storage_full);
    pthread_cond_destroy(&manager_flag);
    pthread_cond_destroy(&worker_signal);

}
