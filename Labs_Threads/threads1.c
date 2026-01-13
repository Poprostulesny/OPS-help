#define _POSIX_C_SOURCE 199309L
#include<pthread.h>
#include<stdio.h>
#include<stdlib.h>
#include<semaphore.h>
#include<time.h>
#include<stddef.h>


/*Write a program that takes 2 input parameters: n,k. where n is the number of processing posix threads and k is the task array size. The main thread fills the task array with random floating point number of range [1,60]. Main thread creates n processing threads. Each processing thread is calculating the square root of random array cell and stores the result in result array, prints: “sqrt(a) = b” (b is a result, a is an input) and then sleeps for 100ms. The programmer must ensure that each cell is calculated only once and each calculation should be mutex protected. The program ends when all cells are calculated. The main thread is waiting for all threads and then prints input and results arrays.

Graded stages #
At each stage run the program with n=3 and k = 7 when presenting the results

Creating n+1 threads. Each thread is printing “*”. Main thread is waiting for all threads.
Main thread is filling the task array and prints it, the processing threads are choosing random array cell number, print its index and exit.
Processing threads are calculating result of one random cell with mutex synchronization, print the result and exit . Main thread is printing both arrays afterwards (after the join). At this stage cells can be calculated more than once but not in parallel.
Each cell is protected by separate mutex. Processing threads are counting remaining cells to compute. If that number reaches 0, threads are terminating otherwise they calculate the next random remaining cell and sleep*/
sem_t sem;
typedef struct arg{
    double *in;
    double *out;
    pthread_mutex_t *mut;
    int k;
}arg;

double sqrt(double x){
    double p = 0;
    double k = x;
    double s;
    double e = 0.001;
    if(x==1){
        return 1.0;
    }
    while(k-p>e){
        s=(p+k)/(double)2;
        if(s*s<x){
            p=s;
        }
        else{
            k=s;
        }
    }
    return s;

}
void * work(void * args){
    arg * input = (arg*)args;
    double *in = input->in;
    double *out = input->out;
    pthread_mutex_t * mut = input->mut;
    int k = input->k;
    

    // printf("I am a thread who chose the cell %d with value %f \n", r, in[r]);
    while(1){
        int err = sem_trywait(&sem);
        if(err!=0){
            return NULL;
        }
        int r = rand()%k;
        pthread_mutex_lock(&mut[r]);
        out[r] = sqrt(in[r]);
        // printf("sqrt(%f) = %f\n", in[r], out[r]);

        pthread_mutex_unlock(&mut[r]); 
    }
    

    


}

int main(int argc, char* argv[]){
    if(argc<3){
        exit(EXIT_FAILURE);
    }
    int n = atoi(argv[1]);
    int k = atoi(argv[2]);
    srand(time(NULL));
    sem_init(&sem, 0, k);
    pthread_t threads[n];

    double res[k];
    double in[k];
    pthread_mutex_t mut[k];
    for(int i=0;i<n;i++){
      
        res[i]=-1;
    }
    for(int i=0;i<k;i++){
          pthread_mutex_init(&mut[i], NULL);
    }

    arg input;
    input.in = in;
    input.out = res;
    input.mut = mut;
    input.k = k;
    struct timespec start, finish;
    for(int i=0;i<k;i++){
        in[i]= (double)(6*1000*1000*1000*1000*1000)*(double)rand()/(double)RAND_MAX;
    }
    clock_gettime(CLOCK_MONOTONIC, &start);
    for(int i=0;i<n;i++){
        int err = pthread_create(&threads[i], NULL, work, (void*)&input);
        if(err!=0){
            exit(EXIT_FAILURE);
        }
    }
    for(int i=0;i<n;i++){
        pthread_join(threads[i], NULL);
    }
    clock_gettime(CLOCK_MONOTONIC,&finish);



    printf("Square root computation:\n");
    for(int i=0;i<k;i++){
        printf("Value %10f: Root: %10f\n", in[i], res[i]);
    }
    double elapsed_time = (finish.tv_sec  - start.tv_sec) * 1e6 +
                      (finish.tv_nsec - start.tv_nsec) / 1e3;   // microseconds
printf("Time elapsed: %f seconds\n", elapsed_time / 1e6);
   
    sem_destroy(&sem);
    exit(EXIT_SUCCESS);
}