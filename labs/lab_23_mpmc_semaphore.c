/*
 * lab_23_mpmc_semaphore.c
 * Topic: MPMC, Semaphores, Monitors
 * Build: gcc -O0 -Wall -pthread -o lab_23 lab_23_mpmc_semaphore.c
 *
 * Author: Arun Singh | arunsingh.in@gmail.com
 * OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
*/
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
static void ps(const char *t){printf("\n========== %s ==========\n",t);}
#define BUFSIZE 5
static int buffer[BUFSIZE];
static sem_t empty_slots,full_slots;
static pthread_mutex_t buf_mtx=PTHREAD_MUTEX_INITIALIZER;
static int in_i=0,out_i=0,total_produced=0,total_consumed=0;
static volatile int stop=0;
static void *producer(void*arg){
    int id=*(int*)arg;
    for(int i=0;i<10&&!stop;i++){
        sem_wait(&empty_slots);
        pthread_mutex_lock(&buf_mtx);
        int val=__sync_fetch_and_add(&total_produced,1);
        buffer[in_i]=val;in_i=(in_i+1)%BUFSIZE;
        printf("  P%d: put %d\n",id,val);
        pthread_mutex_unlock(&buf_mtx);
        sem_post(&full_slots);
        usleep(5000);
    }return NULL;}
static void *consumer(void*arg){
    int id=*(int*)arg;
    for(int i=0;i<10&&!stop;i++){
        sem_wait(&full_slots);
        pthread_mutex_lock(&buf_mtx);
        int val=buffer[out_i];out_i=(out_i+1)%BUFSIZE;
        __sync_fetch_and_add(&total_consumed,1);
        printf("  C%d: got %d\n",id,val);
        pthread_mutex_unlock(&buf_mtx);
        sem_post(&empty_slots);
        usleep(8000);
    }return NULL;}
int main(void){
    printf("=== Lab 23: MPMC Queue, Semaphores ===\n");
    ps("Multi-Producer Multi-Consumer with Semaphores");
    sem_init(&empty_slots,0,BUFSIZE);
    sem_init(&full_slots,0,0);
    int ids[]={0,1,2,3};
    pthread_t p[2],c[2];
    for(int i=0;i<2;i++){pthread_create(&p[i],NULL,producer,&ids[i]);pthread_create(&c[i],NULL,consumer,&ids[i+2]);}
    for(int i=0;i<2;i++){pthread_join(p[i],NULL);pthread_join(c[i],NULL);}
    printf("\n  Total produced: %d, consumed: %d\n",total_produced,total_consumed);
    printf("\n  OBSERVE: Semaphores count available resources (slots/items).\n");
    printf("  sem_wait decrements (blocks at 0), sem_post increments.\n");
    printf("  This is the classic bounded buffer / monitor pattern.\n");
    printf("\n========== Quiz ==========\n");
    printf("Q1. How does a semaphore differ from a mutex?\n");
    printf("Q2. What is a monitor and how do condvar+mutex approximate one?\n");
    printf("Q3. Why do we need both semaphores AND a mutex in this MPMC queue?\n");
    printf("Q4. Can semaphores be used across processes? How?\n");
    return 0;}
