/*
 * lab_21_lock_variations.c
 * Topic: Locking Variations
 * Build: gcc -O0 -Wall -pthread -o lab_21 lab_21_lock_variations.c
 *
 * Author: Arun Singh | arunsingh.in@gmail.com
 * OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
*/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
static void ps(const char *t){printf("\n========== %s ==========\n",t);}
static double now(void){struct timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);return ts.tv_sec*1e9+ts.tv_nsec;}
static pthread_rwlock_t rwl=PTHREAD_RWLOCK_INITIALIZER;
static long shared_data=0;
static int ITERS=500000;
static void *reader(void*a){(void)a;long s=0;
    for(int i=0;i<ITERS;i++){pthread_rwlock_rdlock(&rwl);s+=shared_data;pthread_rwlock_unlock(&rwl);}
    return (void*)s;}
static void *writer(void*a){(void)a;
    for(int i=0;i<ITERS/10;i++){pthread_rwlock_wrlock(&rwl);shared_data++;pthread_rwlock_unlock(&rwl);}
    return NULL;}
int main(void){
    printf("=== Lab 21: Locking Variations ===\n");
    ps("Phase 1: Read-Write Lock");
    printf("  Multiple readers can hold the lock simultaneously.\n");
    printf("  Writers get exclusive access.\n\n");
    pthread_t r[4],w[2];
    double t0=now();
    for(int i=0;i<4;i++)pthread_create(&r[i],NULL,reader,NULL);
    for(int i=0;i<2;i++)pthread_create(&w[i],NULL,writer,NULL);
    for(int i=0;i<4;i++)pthread_join(r[i],NULL);
    for(int i=0;i<2;i++)pthread_join(w[i],NULL);
    printf("  RWLock time: %.1f ms, shared_data=%ld\n",(now()-t0)/1e6,shared_data);
    ps("Phase 2: Trylock (Non-blocking)");
    pthread_mutex_t m=PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock(&m);
    int r2=pthread_mutex_trylock(&m);
    printf("  trylock while held: %s (returns EBUSY=%d)\n",r2?"FAILED":"OK",r2);
    pthread_mutex_unlock(&m);
    r2=pthread_mutex_trylock(&m);
    printf("  trylock while free: %s\n",r2?"FAILED":"OK");
    if(!r2)pthread_mutex_unlock(&m);
    ps("Phase 3: Recursive Mutex");
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr,PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_t rm;
    pthread_mutex_init(&rm,&attr);
    pthread_mutex_lock(&rm);
    pthread_mutex_lock(&rm);
    printf("  Recursive mutex: locked twice without deadlock.\n");
    pthread_mutex_unlock(&rm);pthread_mutex_unlock(&rm);
    printf("\n========== Quiz ==========\n");
    printf("Q1. When is a rwlock better than a mutex? When is it worse?\n");
    printf("Q2. What problem does trylock solve? Give a real-world example.\n");
    printf("Q3. Why are recursive mutexes considered a code smell?\n");
    printf("Q4. What is writer starvation in rwlocks and how can it be prevented?\n");
    return 0;}
