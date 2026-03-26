/*
 * lab_24_lockfree.c
 * Topic: Lock-free, CAS, RW Locks
 * Build: gcc -O0 -Wall -pthread -o lab_24 lab_24_lockfree.c
 *
 * Author: Arun Singh | arunsingh.in@gmail.com
 * OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
*/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <pthread.h>
#include <time.h>
static void ps(const char *t){printf("\n========== %s ==========\n",t);}
static double now(void){struct timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);return ts.tv_sec*1e9+ts.tv_nsec;}
/* Lock-free stack using CAS */
typedef struct node{int val;struct node*next;}node_t;
static _Atomic(node_t*) stack_top=NULL;
static void push(int val){
    node_t *n=malloc(sizeof(*n));n->val=val;
    node_t *old;
    do{old=atomic_load(&stack_top);n->next=old;}
    while(!atomic_compare_exchange_weak(&stack_top,&old,n));
}
static int pop(int *out){
    node_t *old;
    do{old=atomic_load(&stack_top);if(!old)return 0;}
    while(!atomic_compare_exchange_weak(&stack_top,&old,old->next));
    *out=old->val;free(old);return 1;
}
static void *pusher(void*a){(void)a;for(int i=0;i<100000;i++)push(i);return NULL;}
int main(void){
    printf("=== Lab 24: Lock-free Primitives, CAS ===\n");
    ps("Phase 1: Compare-and-Swap (CAS) Lock-free Stack");
    printf("  CAS: atomically compare and swap a memory location.\n");
    printf("  If current==expected, write new value. Else retry.\n\n");
    pthread_t t[4];
    double t0=now();
    for(int i=0;i<4;i++)pthread_create(&t[i],NULL,pusher,NULL);
    for(int i=0;i<4;i++)pthread_join(t[i],NULL);
    printf("  Pushed 400000 items in %.1f ms\n",(now()-t0)/1e6);
    int count=0,val;while(pop(&val))count++;
    printf("  Popped %d items (should be 400000)\n",count);
    ps("Phase 2: Memory Ordering");
    printf("  C11 memory orders: relaxed, acquire, release, seq_cst\n");
    printf("  x86 provides strong ordering (TSO) — most ops are seq_cst by default.\n");
    printf("  ARM/RISC-V are weakly ordered — need explicit barriers.\n");
    printf("  Kernel uses: smp_mb(), smp_rmb(), smp_wmb(), READ_ONCE(), WRITE_ONCE()\n");
    ps("Phase 3: ABA Problem");
    printf("  CAS can suffer from ABA: value changes A->B->A, CAS sees A and succeeds\n");
    printf("  but the state has changed. Solutions: tagged pointers, hazard pointers, RCU.\n");
    printf("\n========== Quiz ==========\n");
    printf("Q1. Explain CAS in your own words. What makes it 'lock-free'?\n");
    printf("Q2. What is the ABA problem? Give a concrete example with the stack above.\n");
    printf("Q3. Why does ARM need more memory barriers than x86?\n");
    printf("Q4. What is a memory model and why does C11 define one?\n");
    return 0;}
