#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include "../mypthread.h"



long double pi=0.0;
pthread_mutex_t mutex;
long double intervals;
int threadNumber;
pthread_t *threads;
int *tID;



void computePI(void *id){
    
    long double x, breadth, sum=0;

    int i, tID=*((int*)id);

    breadth=1.0/intervals;

    for(i=tID;i<intervals;i+=threadNumber){

        x=(i+0.5)*breadth;
        sum+=4.0/(1.0+x*x);
    }

    sum*=breadth;

    pthread_mutex_lock(&mutex);
    pi+=sum;
    pthread_mutex_unlock(&mutex);

    return NULL;
}

int main(int argc, char **argv){

    void *retval;
    int i;

    threads=(pthread_t*)malloc(threadNumber*sizeof(pthread_t));

    if(argc==3){

        intervals=atoi(argv[1]);
        threadNumber=atoi(argv[2]);
        threads=malloc(threadNumber*sizeof(pthread_t));
        tID=malloc(threadNumber*sizeof(int));
        pthread_mutex_init(&mutex, NULL);

        for(i=0;i<threadNumber;i++){

            tID[i]=i;
            pthread_create(&threads[i],NULL, &computePI, tID+i);
        }

        for(i=0;i<threadNumber;i++){

            pthread_join(threads[i], &retval);
        }

        printf("Estimation of pi is %Lf \n", pi);
        printf("(actual pi value is 3.141592653589793238462643383279...)\n");

        free(threads);
    }
    else{
              printf("Usage: ./a.out <numIntervals> <numThreads>");    
    }
    return 0;
}

