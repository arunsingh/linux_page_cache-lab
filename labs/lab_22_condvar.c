/*
 * lab_22_condvar.c
 * Topic: Condition Variables
 * Build: gcc -O0 -Wall -pthread -o lab_22 lab_22_condvar.c
 *
 * Author: Arun Singh | arunsingh.in@gmail.com
 * OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
*/
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
static void ps(const char *t){printf("\n========== %s ==========\n",t);}
#define BUFSIZE 8
static int buffer[BUFSIZE],count=0,in_idx=0,out_idx=0;
static pthread_mutex_t mtx=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t not_full=PTHREAD_COND_INITIALIZER;
static pthread_cond_t not_empty=PTHREAD_COND_INITIALIZER;
static int done=0;
static void produce(int val){
    pthread_mutex_lock(&mtx);
    while(count==BUFSIZE)pthread_cond_wait(&not_full,&mtx);
    buffer[in_idx]=val;in_idx=(in_idx+1)%BUFSIZE;count++;
    printf("  [P] produced %d (count=%d)\n",val,count);
    pthread_cond_signal(&not_empty);
    pthread_mutex_unlock(&mtx);
}
static int consume(void){
    pthread_mutex_lock(&mtx);
    while(count==0&&!done)pthread_cond_wait(&not_empty,&mtx);
    if(count==0&&done){pthread_mutex_unlock(&mtx);return -1;}
    int val=buffer[out_idx];out_idx=(out_idx+1)%BUFSIZE;count--;
    printf("  [C] consumed %d (count=%d)\n",val,count);
    pthread_cond_signal(&not_full);
    pthread_mutex_unlock(&mtx);
    return val;
}
static void *producer(void*a){(void)a;for(int i=0;i<20;i++){produce(i);usleep(10000);}
    pthread_mutex_lock(&mtx);done=1;pthread_cond_broadcast(&not_empty);pthread_mutex_unlock(&mtx);return NULL;}
static void *consumer(void*a){(void)a;while(consume()>=0);return NULL;}
int main(void){
    printf("=== Lab 22: Condition Variables ===\n");
    ps("Producer-Consumer with Bounded Buffer");
    printf("  Buffer size: %d\n  Producer: 20 items\n  Consumer: 2 threads\n\n",BUFSIZE);
    pthread_t p,c1,c2;
    pthread_create(&p,NULL,producer,NULL);
    pthread_create(&c1,NULL,consumer,NULL);
    pthread_create(&c2,NULL,consumer,NULL);
    pthread_join(p,NULL);pthread_join(c1,NULL);pthread_join(c2,NULL);
    printf("\n  OBSERVE: Threads sleep when buffer is full/empty, wake on signal.\n");
    printf("  This avoids busy-waiting (unlike spinlocks).\n");
    printf("\n========== Quiz ==========\n");
    printf("Q1. Why must cond_wait be called inside a while loop, not an if?\n");
    printf("Q2. What is a spurious wakeup?\n");
    printf("Q3. When should you use broadcast vs signal?\n");
    printf("Q4. How does the kernel implement futex-based condition variables?\n");
    return 0;}
