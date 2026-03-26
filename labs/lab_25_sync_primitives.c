/*
 * lab_25_sync_primitives.c
 * Topic: Sync: acquire/release, sleep/wakeup
 * Build: gcc -O0 -Wall -pthread -o lab_25 lab_25_sync_primitives.c
 *
 * Author: Arun Singh | arunsingh.in@gmail.com
 * OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
*/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <stdatomic.h>
#include <errno.h>
static void ps(const char *t){printf("\n========== %s ==========\n",t);}
static atomic_int futex_val=0;
static int futex_wait(atomic_int*addr,int expected){
    return (int)syscall(SYS_futex,addr,FUTEX_WAIT,expected,NULL,NULL,0);}
static int futex_wake(atomic_int*addr,int count){
    return (int)syscall(SYS_futex,addr,FUTEX_WAKE,count,NULL,NULL,0);}
static void *sleeper(void*a){(void)a;
    printf("  Sleeper: waiting for futex_val to become 1...\n");
    while(atomic_load(&futex_val)==0)futex_wait(&futex_val,0);
    printf("  Sleeper: woke up! futex_val=%d\n",atomic_load(&futex_val));
    return NULL;}
int main(void){
    printf("=== Lab 25: Synchronization: acquire/release, sleep/wakeup ===\n");
    ps("Phase 1: Futex — Foundation of Linux Synchronization");
    printf("  futex = Fast Userspace muTEX\n");
    printf("  Uncontended case: pure userspace atomic ops (no syscall).\n");
    printf("  Contended case: syscall to kernel for sleep/wakeup.\n\n");
    pthread_t t;
    pthread_create(&t,NULL,sleeper,NULL);
    usleep(100000);
    printf("  Main: setting futex_val=1 and waking sleeper\n");
    atomic_store(&futex_val,1);
    futex_wake(&futex_val,1);
    pthread_join(t,NULL);
    ps("Phase 2: xv6-style sleep/wakeup Concepts");
    printf("  In xv6 (teaching OS):\n");
    printf("    sleep(channel, lock): release lock, add to sleep queue, yield CPU\n");
    printf("    wakeup(channel): wake all processes sleeping on this channel\n");
    printf("  Linux equivalent: wait_event() / wake_up() on wait queues.\n");
    printf("  The 'lost wakeup' problem: must hold lock when checking condition.\n");
    ps("Phase 3: Acquire/Release Semantics");
    printf("  acquire: all subsequent reads/writes stay AFTER this point\n");
    printf("  release: all preceding reads/writes stay BEFORE this point\n");
    printf("  Together they create a happens-before relationship.\n");
    printf("  Linux: spin_lock = acquire, spin_unlock = release.\n");
    printf("\n========== Quiz ==========\n");
    printf("Q1. Why is futex faster than a pure kernel mutex for uncontended locks?\n");
    printf("Q2. What is the 'lost wakeup' problem and how is it prevented?\n");
    printf("Q3. Explain acquire/release semantics with a producer-consumer example.\n");
    printf("Q4. How does the kernel wait_event() macro prevent lost wakeups?\n");
    return 0;}
