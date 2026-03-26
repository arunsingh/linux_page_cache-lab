/*
 * lab_36_rcu_lockfree.c
 * Topic: RCU, Lock-free Coordination
 * Build: gcc -O0 -Wall -pthread -o lab_36 lab_36_rcu_lockfree.c
 *
 * Author: Arun Singh | arunsingh.in@gmail.com
 * OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
*/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <pthread.h>
#include <unistd.h>
static void ps(const char *t){printf("\n========== %s ==========\n",t);}
/* Simplified userspace RCU-like pattern */
typedef struct data{int version;int value;}data_t;
static _Atomic(data_t*) global_data;
static void rcu_read(int id){
    data_t *p=atomic_load_explicit(&global_data,memory_order_acquire);
    printf("  Reader %d: version=%d value=%d\n",id,p->version,p->value);
}
static void rcu_update(int new_val){
    data_t *old=atomic_load(&global_data);
    data_t *new_d=malloc(sizeof(*new_d));
    new_d->version=old->version+1;new_d->value=new_val;
    atomic_store_explicit(&global_data,new_d,memory_order_release);
    usleep(100000); /* grace period: wait for all readers to finish with old */
    free(old);
    printf("  Writer: updated to version=%d value=%d\n",new_d->version,new_val);
}
int main(void){
    printf("=== Lab 36: RCU, Lock-free Multiprocessor Coordination ===\n");
    data_t *init=malloc(sizeof(*init));init->version=0;init->value=0;
    atomic_store(&global_data,init);
    ps("Phase 1: RCU Pattern (simplified)");
    printf("  Read-Copy-Update: readers never block, writers copy-then-swap.\n");
    printf("  Grace period: writer waits until all readers using old data finish.\n\n");
    rcu_read(0);rcu_update(42);rcu_read(1);rcu_update(99);rcu_read(2);
    ps("Phase 2: Kernel RCU");
    printf("  Linux kernel uses RCU extensively:\n");
    printf("    - Routing tables, firewall rules (read-heavy, rare updates)\n");
    printf("    - Module lists, PID hash table\n");
    printf("    - rcu_read_lock() / rcu_read_unlock() are essentially free (preempt_disable)\n");
    printf("    - synchronize_rcu() waits for grace period\n");
    printf("    - call_rcu() defers cleanup to after grace period\n");
    ps("Phase 3: Memory Barriers in Practice");
    printf("  smp_mb()  — full memory barrier\n");
    printf("  smp_rmb() — read barrier (loads before stay before)\n");
    printf("  smp_wmb() — write barrier (stores before stay before)\n");
    printf("  On x86 (TSO): smp_rmb/wmb are compiler barriers only.\n");
    printf("  On ARM: actual hardware barriers emitted.\n");
    printf("\n========== Quiz ==========\n");
    printf("Q1. Why is RCU so fast for readers?\n");
    printf("Q2. What is a grace period and how does the kernel determine it?\n");
    printf("Q3. When is RCU better than rwlocks?\n");
    printf("Q4. Why does smp_rmb() do nothing on x86 but emit instructions on ARM?\n");
    return 0;}
