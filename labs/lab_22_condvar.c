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

    /* WHY: pthread_cond_wait is implemented using two futexes:
     *      1. The mutex futex (FUTEX_LOCK_PI or FUTEX_WAIT)
     *      2. The condvar futex (FUTEX_WAIT)
     *      pthread_cond_wait atomically releases the mutex and waits on the condvar.
     *      This atomicity is critical: without it, a signal between unlock and wait
     *      would be lost.  The kernel's FUTEX_REQUEUE operation moves waiters from
     *      the condvar queue to the mutex queue atomically.
     *
     * WHY: Spurious wakeup: POSIX allows pthread_cond_wait to return even when no
     *      signal/broadcast was called.  This can happen due to:
     *      - EINTR from a signal delivered while waiting
     *      - Implementation-specific reasons on some platforms
     *      This is why the while(condition) loop is MANDATORY, not an if().
     *
     * WHY: futex wait/wake in the kernel:
     *      futex(FUTEX_WAIT, addr, val): if *addr == val, sleep on addr's wait queue.
     *      futex(FUTEX_WAKE, addr, N): wake N waiters on addr's wait queue.
     *      futex(FUTEX_REQUEUE, addr1, N, addr2): wake N from addr1, move rest to addr2.
     *      REQUEUE is used by pthread_cond_signal to avoid thundering herd.
     */
    ps("Phase 2: Bounded Producer-Consumer with Separate Condition Variables");
    printf("  This lab already implements the classic bounded P-C pattern.\n");
    printf("  Two condvars: not_full (producer waits here) and not_empty (consumer waits).\n\n");
    printf("  Key design points:\n");
    printf("    1. while(count==0) not if(count==0) -- handles spurious wakeups\n");
    printf("    2. cond_signal vs cond_broadcast:\n");
    printf("       - signal: wake ONE waiter (enough for single-item producers)\n");
    printf("       - broadcast: wake ALL waiters (needed when condition changes for many)\n");
    printf("    3. 'done' flag broadcast: must broadcast to wake BOTH consumers at end\n\n");
    printf("  futex_wait/wake trace:\n");
    printf("    cond_wait  -> FUTEX_WAIT on condvar, then FUTEX_LOCK on mutex\n");
    printf("    cond_signal -> FUTEX_REQUEUE: move 1 waiter from condvar to mutex queue\n");
    printf("    cond_broadcast -> FUTEX_REQUEUE: move ALL waiters to mutex queue\n");

    /* === EXERCISE ===
     * Try these hands-on tasks:
     * 1. Implement a multi-producer multi-consumer (MPMC) version:
     *    4 producers, 4 consumers, buffer size 16.  Verify no items are lost or
     *    duplicated.  Add a global items_produced and items_consumed counter.
     *    At the end: items_produced == items_consumed (use atomics for counting).
     * 2. Change 'while(count==0)' to 'if(count==0)' and add random usleep()
     *    in the producer to trigger spurious-wakeup-like behavior.  Observe
     *    what happens: consumers may wake with count==0 and try to consume nothing.
     *    This demonstrates why the while loop is mandatory.
     * 3. Modern (futex wait/wake in kernel): use strace -e futex on this program.
     *    You will see FUTEX_WAIT calls (consumer sleeping) and FUTEX_WAKE/REQUEUE
     *    calls (producer signaling).  Count the total futex calls and compare to
     *    the number of produced items.  Each produce() + consume() pair may use
     *    0-2 futex calls depending on whether the condvar queue has waiters.
     *
     * OBSERVE: broadcast wakes ALL sleeping consumers, but only one can hold the
     *          mutex at a time.  The others re-check the condition and go back to
     *          sleep.  This is the "thundering herd" problem -- use signal when
     *          only one waiter can make progress (e.g., buffer has room for 1 item).
     * WHY:     The atomicity of cond_wait (release mutex + wait) is implemented
     *          via futex_requeue: the mutex and condvar share a futex table.
     *          Releasing the mutex and queuing on the condvar is one atomic kernel op.
     */
    printf("\n========== Hands-On Exercise ==========\n");
    printf("1. Implement MPMC: 4 producers, 4 consumers; verify no lost/duplicate items.\n");
    printf("2. Change while to if in cond_wait loop; observe potential consume of empty buffer.\n");
    printf("3. Modern (futex): strace -e futex ./lab_22 to count FUTEX_WAIT/WAKE;\n");
    printf("   compare futex call count to items produced; observe REQUEUE for broadcast.\n");

    printf("\n========== Quiz ==========\n");
    printf("Q1. Why must cond_wait be called inside a while loop, not an if?\n");
    printf("Q2. What is a spurious wakeup?\n");
    printf("Q3. When should you use broadcast vs signal?\n");
    printf("Q4. How does the kernel implement futex-based condition variables?\n");
    printf("Q5. What is the atomicity requirement of pthread_cond_wait?\n");
    printf("    Why would a non-atomic unlock+wait have a race condition?\n");
    printf("Q6. What is the FUTEX_REQUEUE operation and how does it prevent\n");
    printf("    thundering herd when pthread_cond_broadcast is called?\n");
    printf("Q7. How would you implement a condition variable using only semaphores?\n");
    printf("    What is the Mesa semantics vs Hoare semantics distinction?\n");
    return 0;
}
