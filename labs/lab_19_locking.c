/*
 * lab_19.c
 * Topic: Locking
 * Build: gcc -O0 -Wall -pthread -o lab_19 lab_19_*.c
 *
 * Author: Arun Singh | arunsingh.in@gmail.com
 * OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
static void print_section(const char *t){printf("\n========== %s ==========\n",t);}
static double now_ns(void){struct timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);return ts.tv_sec*1e9+ts.tv_nsec;}
static pthread_mutex_t mtx=PTHREAD_MUTEX_INITIALIZER;
static pthread_spinlock_t spin;
static long counter_mtx=0,counter_spin=0;
static int ITERS=2000000;
static void *mutex_worker(void*a){
    (void)a;for(int i=0;i<ITERS;i++){pthread_mutex_lock(&mtx);counter_mtx++;pthread_mutex_unlock(&mtx);}
    return NULL;
}
static void *spin_worker(void*a){
    (void)a;for(int i=0;i<ITERS;i++){pthread_spin_lock(&spin);counter_spin++;pthread_spin_unlock(&spin);}
    return NULL;
}
int main(void){
    printf("=== Lab 19: Locking ===\n");
    pthread_spin_init(&spin,PTHREAD_PROCESS_PRIVATE);
    int T=4;pthread_t threads[4];
    print_section("Phase 1: Mutex Performance");
    double t0=now_ns();
    for(int i=0;i<T;i++)pthread_create(&threads[i],NULL,mutex_worker,NULL);
    for(int i=0;i<T;i++)pthread_join(threads[i],NULL);
    double t_mtx=now_ns()-t0;
    printf("  Mutex: counter=%ld, time=%.1f ms, per_op=%.0f ns\n",counter_mtx,t_mtx/1e6,t_mtx/(ITERS*T));
    print_section("Phase 2: Spinlock Performance");
    t0=now_ns();
    for(int i=0;i<T;i++)pthread_create(&threads[i],NULL,spin_worker,NULL);
    for(int i=0;i<T;i++)pthread_join(threads[i],NULL);
    double t_spin=now_ns()-t0;
    printf("  Spinlock: counter=%ld, time=%.1f ms, per_op=%.0f ns\n",counter_spin,t_spin/1e6,t_spin/(ITERS*T));
    printf("\n  OBSERVE: Spinlocks busy-wait (waste CPU) but avoid syscall overhead.\n");
    printf("  Mutexes sleep on contention (futex) — better for long critical sections.\n");
    printf("  Kernel uses spinlocks in interrupt context (cannot sleep).\n");
    printf("\n========== Quiz ==========\n");
    printf("Q1. When should you use a spinlock vs a mutex?\n");
    printf("Q2. What is a futex and how does pthread_mutex use it?\n");
    printf("Q3. Why cant the kernel use mutexes in interrupt handlers?\n");
    printf("Q4. What is priority inversion and how can it occur with mutexes?\n");
    return 0;
}
