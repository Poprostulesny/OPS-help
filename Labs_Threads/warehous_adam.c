#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#define MAX_NUM 256

int max_capacity;
int curr_capacity;
sigset_t mask;
volatile int work = 1;

int arr[MAX_NUM][MAX_NUM];
pthread_mutex_t mutex_arr[MAX_NUM][MAX_NUM];
pthread_mutex_t mutex_capacity;


typedef struct{
    pthread_t tid;
    unsigned int seed;
    int n,m;
}dataThread_Dostawcy_t;

typedef struct{
    pthread_t tid;
    unsigned int seed;
    int n,m;
}dataThread_Odbiorcy_t;

int r(int a, int b, unsigned int *seed){
    return a + rand_r(seed) % (b-a+1); 
}

void *fun_dostawcy(void *voidPtr){
    dataThread_Dostawcy_t *data = (dataThread_Dostawcy_t *)voidPtr;
    while(work){
        struct timespec req = {1,0};
        nanosleep(&req, NULL);
        while(1){
            pthread_mutex_lock(&mutex_capacity);
            if(curr_capacity < max_capacity){
                pthread_mutex_unlock(&mutex_capacity);
                int x = r(0,data->n-1,&data->seed);
                int y = r(0,data->m-1,&data->seed);
                pthread_mutex_lock(&mutex_arr[x][y]);
                if(arr[x][y] == 0){
                    arr[x][y] = 1;
                    printf("Wstawiam towar na (%d, %d) \n", x, y);
                    pthread_mutex_lock(&mutex_capacity);
                    curr_capacity++;
                    pthread_mutex_unlock(&mutex_capacity); 
                }
                pthread_mutex_unlock(&mutex_arr[x][y]);
            }else{
                pthread_mutex_unlock(&mutex_capacity);
                fprintf(stderr, "Max capacity!\n");
                printf("czekam na sygnal do pracy od glownego \n");
                int sig;
                if(sigwait(&mask, &sig) != 0) perror("sigwait");
                if(sig == SIGUSR1) {
                    printf("wracam do pracy wstawiania towarow \n"); 
                    break;
                }
                printf("teroretcznie skonczylem czekac na sygnal\n");
                break;
            }
        }

    }
    return NULL;
}

void *fun_odbiorcy(void *voidPtr){
    dataThread_Odbiorcy_t *data = (dataThread_Odbiorcy_t *) voidPtr;
    while(work){
        struct timespec req = {3,0};
        nanosleep(&req, NULL);
        int x = r(0,data->n-1, &data->seed);
        int cnt = 0;
        for(int i=0; data->m>i; i++){
            pthread_mutex_lock(&mutex_arr[x][i]);
            if(arr[x][i] == 1){
                cnt++;
            }
            arr[x][i] = 0;
            pthread_mutex_unlock(&mutex_arr[x][i]);
        }
        printf("Odbiora usunal %d elementow w rzedzie %d \n", cnt, x);
        pthread_mutex_lock(&mutex_capacity);
        curr_capacity -= cnt;
        pthread_mutex_unlock(&mutex_capacity);

        printf("wysylam syngal do maina \n");
        kill(getpid(), SIGUSR2);
        printf("wyslalem sygnal do maina \n");
    }


    return NULL;
}

int main(int argc, char *argv[]){
    sigemptyset(&mask);       
    sigaddset(&mask, SIGUSR1);
    sigaddset(&mask, SIGUSR2);
    sigaddset(&mask, SIGINT);

    if (pthread_sigmask(SIG_BLOCK, &mask, NULL) != 0) {
        perror("Bląd maskowania");
        return 1;
    }

    srand(time(NULL));
    (void)argc;
    int n = atoi(argv[1]);
    int m = atoi(argv[2]);
    max_capacity = n*m;
    curr_capacity = 0;

    for(int i=0; n>i; i++){
        for(int j=0; m>j; j++){
            arr[i][j] = 0;
            pthread_mutex_init(&mutex_arr[i][j], NULL);
        }
    }
    pthread_mutex_init(&mutex_capacity, NULL);
    dataThread_Dostawcy_t* dostawcy = (dataThread_Dostawcy_t*)malloc(sizeof(dataThread_Dostawcy_t) * 3);
    dataThread_Odbiorcy_t *odbiorcy = (dataThread_Odbiorcy_t*)malloc(sizeof(dataThread_Odbiorcy_t) * 2);
    for(int i=0; 3>i; i++){
        dostawcy[i] = (dataThread_Dostawcy_t){
            .tid = i,
            .seed = (unsigned int)rand(),
            .n = n,
            .m = m
        };
        pthread_create(&dostawcy[i].tid, NULL, fun_dostawcy, &dostawcy[i]);
    }


    for(int i=0; 2>i; i++){
        odbiorcy[i] = (dataThread_Odbiorcy_t){
            .tid = i,
            .seed = (unsigned int)rand(),
            .n = n,
            .m = m
        };
        pthread_create(&odbiorcy[i].tid, NULL, fun_odbiorcy, &odbiorcy[i]); 
    }


    for(int i=0; 3>i; i++){
        pthread_join(dostawcy[i].tid, NULL);
    }
    for(int i=0; 2>i; i++){
        pthread_join(odbiorcy[i].tid, NULL);
    }

    while (work) {
        int sig;
        if (sigwait(&mask, &sig) != 0) perror("sigwait");
        if(sig == SIGUSR2){
            printf("Dostalem sigusr1\n");
            kill(getpid(), SIGUSR1);
        }else if(sig == SIGINT){
            work = 0;
            break;
        }
    }
    
    free(odbiorcy);
    free(dostawcy);
    for(int i=0; n>i; i++){
        for(int j=0; m>j; j++){
            pthread_mutex_destroy(&mutex_arr[i][j]);
        }
    }
}