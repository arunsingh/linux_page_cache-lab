/*
 * lab_20.c
 * Topic: Fine-grained locking and its challenges
 * Build: gcc -O0 -Wall -pthread -o lab_20 lab_20_*.c
 *
 * Author: Arun Singh | arunsingh.in@gmail.com
 * OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
static void print_section(const char *t){printf("\n========== %s ==========\n",t);}
static double now_ns(void){struct timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);return ts.tv_sec*1e9+ts.tv_nsec;}
#define NBUCKETS 64
#define NITEMS 1000000
static pthread_mutex_t global_lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t bucket_locks[NBUCKETS];
static long global_table[NBUCKETS];
static long fine_table[NBUCKETS];

static void *coarse_worker(void*a){
    (void)a;
    for(int i=0;i<NITEMS;i++){
        pthread_mutex_lock(&global_lock);
        global_table[i%NBUCKETS]++;
        pthread_mutex_unlock(&global_lock);
    }
    return NULL;
}
static void *fine_worker(void*a){
    (void)a;
    for(int i=0;i<NITEMS;i++){
        int b=i%NBUCKETS;
        pthread_mutex_lock(&bucket_locks[b]);
        fine_table[b]++;
        pthread_mutex_unlock(&bucket_locks[b]);
    }
    return NULL;
}
int main(void){
    printf("=== Lab 20: Fine-grained Locking ===\n");
    for(int i=0;i<NBUCKETS;i++)pthread_mutex_init(&bucket_locks[i],NULL);
    int T=4;pthread_t threads[4];
    print_section("Phase 1: Coarse-grained (single lock)");
    double t0=now_ns();
    for(int i=0;i<T;i++)pthread_create(&threads[i],NULL,coarse_worker,NULL);
    for(int i=0;i<T;i++)pthread_join(threads[i],NULL);
    printf("  Time: %.1f ms\n",(now_ns()-t0)/1e6);
    print_section("Phase 2: Fine-grained (per-bucket locks)");
    t0=now_ns();
    for(int i=0;i<T;i++)pthread_create(&threads[i],NULL,fine_worker,NULL);
    for(int i=0;i<T;i++)pthread_join(threads[i],NULL);
    printf("  Time: %.1f ms\n",(now_ns()-t0)/1e6);
    printf("\n  OBSERVE: Fine-grained locking allows parallel access to different buckets.\n");
    printf("  Tradeoff: more locks = more memory, risk of deadlock, harder to reason about.\n");
    printf("  Kernel example: per-inode locks vs global filesystem lock.\n");
    printf("\n========== Quiz ==========\n");
    printf("Q1. Why is fine-grained locking faster under contention?\n");
    printf("Q2. What is lock ordering and why does it prevent deadlock?\n");
    printf("Q3. Give a kernel example of fine-grained locking.\n");
    printf("Q4. What is the tradeoff between lock granularity and complexity?\n");
    return 0;
}
