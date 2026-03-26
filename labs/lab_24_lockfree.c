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
    printf("  but the state has changed. Solutions: tagged pointers, hazard pointers, RCU.\n\n");

    /* WHY: Tagged pointers solve ABA: combine the pointer with a version counter.
     *      On x86_64, virtual addresses only use 48 bits (or 57 with LA57).
     *      The top 16 bits can store a generation counter.
     *      128-bit CAS (CMPXCHG16B on x86_64) compares both pointer and tag atomically.
     *      DPDK (Data Plane Development Kit) uses this for lock-free ring buffers
     *      in network packet processing -- millions of enqueue/dequeue per second.
     *
     * WHY: Hazard pointers: each thread holds a "hazard pointer" to the node it is
     *      currently reading.  The free list checks hazard pointers before freeing.
     *      If a node is in any hazard pointer, it cannot be freed.
     *      This solves both ABA (version-based) and memory reclamation problems.
     *      Folly (Facebook), jemalloc, and Linux bpf_cpumask use hazard pointer patterns.
     */
    ps("Phase 4: ABA Solution with Tagged Pointers");
    {
        /* Demonstrate the concept of a tagged pointer */
        uintptr_t ptr_value = (uintptr_t)0x7fffc0001234UL; /* typical user VA */
        unsigned long tag = 7;
        /* Pack: lower 48 bits = pointer, upper 16 bits = tag (conceptual) */
        /* On x86_64 with 48-bit VA, upper 16 bits are sign-extended to 1s or 0s.
         * In practice, use __uint128_t for CMPXCHG16B on x86_64. */
        printf("  Tagged pointer concept:\n");
        printf("    ptr  = 0x%lx (48-bit VA)\n", ptr_value);
        printf("    tag  = %lu (version counter, incremented on each successful CAS)\n", tag);
        printf("    packed: if CAS checks both ptr AND tag, ABA cannot fool it\n");
        printf("    because tag=7 != tag=5 even if ptr is the same address.\n\n");
        printf("  x86_64 CMPXCHG16B: atomically compare+swap 16 bytes (ptr + 8-byte tag).\n");
        printf("  DPDK rte_ring uses this for lock-free packet ring with ABA protection.\n");
    }

    /* === EXERCISE ===
     * Try these hands-on tasks:
     * 1. Implement a lock-free stack with ABA protection using __uint128_t (CMPXCHG16B):
     *    struct tagged_ptr { node_t *ptr; uint64_t tag; };  (16 bytes total)
     *    On push/pop, increment tag on each successful CAS.
     *    Compile with -mcx16 to enable CMPXCHG16B.  Verify: no ABA corruption.
     * 2. Demonstrate the ABA problem with the CURRENT stack implementation:
     *    a) Thread A pops node X (reads X.next = Y, paused before CAS)
     *    b) Thread B pops X, pops Y, pushes X back
     *    c) Thread A's CAS succeeds (stack_top still == X) but sets top to Y
     *       which is now a dangling/freed pointer.  This is use-after-free.
     * 3. Modern (lock-free in DPDK): read DPDK's rte_ring implementation at
     *    lib/ring/rte_ring.h in the DPDK source.  It uses a power-of-2 ring with
     *    separate producer/consumer head/tail pointers, CAS for multi-thread safety.
     *    This powers 100 Gbps packet processing without any locks.
     *
     * OBSERVE: The CAS loop in push() is "lock-free" but not "wait-free".
     *          Lock-free: at least one thread makes progress each step.
     *          Wait-free: every thread makes progress in bounded steps.
     *          Under very high contention, one thread may retry many times.
     * WHY:     C11 atomic_compare_exchange_weak is allowed to fail spuriously
     *          (on RISC architectures using LL/SC instead of CAS).  Use the
     *          _weak version in a loop; use _strong when a single attempt is made.
     */
    printf("\n========== Hands-On Exercise ==========\n");
    printf("1. Implement ABA-safe stack with __uint128_t tagged pointer (CMPXCHG16B);\n");
    printf("   compile with -mcx16; verify no ABA corruption under concurrent access.\n");
    printf("2. Demonstrate ABA in the current stack: thread interleaving that causes\n");
    printf("   dangling pointer dereference (use-after-free scenario).\n");
    printf("3. Modern (DPDK): study rte_ring lock-free ring buffer;\n");
    printf("   explain how it achieves 100 Gbps packet processing without locks.\n");

    printf("\n========== Quiz ==========\n");
    printf("Q1. Explain CAS in your own words. What makes it 'lock-free'?\n");
    printf("Q2. What is the ABA problem? Give a concrete example with the stack above.\n");
    printf("Q3. Why does ARM need more memory barriers than x86?\n");
    printf("Q4. What is a memory model and why does C11 define one?\n");
    printf("Q5. What is the difference between lock-free and wait-free algorithms?\n");
    printf("    Give an example of each from this lab.\n");
    printf("Q6. How do tagged pointers (double-word CAS / CMPXCHG16B) solve the ABA problem?\n");
    printf("Q7. How does DPDK rte_ring achieve millions of lock-free enqueue/dequeue\n");
    printf("    per second for packet processing without any mutex or spinlock?\n");
    return 0;
}
