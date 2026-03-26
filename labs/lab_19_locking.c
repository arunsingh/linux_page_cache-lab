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
    /* WHY: glibc's pthread_mutex is implemented using futexes (Fast Userspace muTEXes,
     *      Linux syscall sys_futex).  In the uncontended case (lock is free), the mutex
     *      is acquired with a single atomic CAS in userspace -- no syscall needed.
     *      Only on contention does the thread call futex(FUTEX_WAIT) to sleep.
     *      On unlock, if waiters exist, futex(FUTEX_WAKE) wakes one.
     *      This is the "fast path" that makes pthread_mutex competitive with spinlocks
     *      for low-contention workloads.  Linux's futex mechanism was introduced in 2.6.
     *
     * WHY: Deadlock with two mutexes: thread A holds lock1, waits for lock2;
     *      thread B holds lock2, waits for lock1.  Both spin forever.
     *      Prevention: always acquire locks in the SAME order (lock ordering).
     *      Priority inversion: low-priority thread holds a lock that a high-priority
     *      thread needs.  High-priority thread is blocked by low-priority one.
     *      Solution: Priority Inheritance (PI) mutex -- glibc supports PTHREAD_PRIO_INHERIT.
     */
    print_section("Phase 3: Futex-Based Mutex Internals");
    printf("  pthread_mutex uncontended path:\n");
    printf("    1. atomic CAS on mutex->__lock field (userspace only, no syscall)\n");
    printf("    2. If CAS succeeds: lock acquired in ~5-10 ns\n");
    printf("    3. If CAS fails: futex(FUTEX_WAIT) syscall to sleep (~1-5 us)\n\n");
    printf("  pthread_mutex unlock:\n");
    printf("    1. Atomic store: set __lock = 0\n");
    printf("    2. If waiters: futex(FUTEX_WAKE, 1) to wake one waiter\n\n");
    printf("  Spinlock uncontended: ~5-10 ns (compare-and-swap loop)\n");
    printf("  Spinlock contended: burns CPU cycles (bad for long waits or high-core count)\n");
    printf("  Rule of thumb: use spinlock only if critical section < ~50 ns.\n");

    /* Show the actual per-op timing comparison */
    printf("\n  Recap:\n");
    printf("    Mutex uncontended:  ~20-50 ns/op (futex fast path)\n");
    printf("    Mutex contended:    ~1-5 us/op (futex_wait syscall + context switch)\n");
    printf("    Spinlock uncontended: ~10-20 ns/op (CAS)\n");
    printf("    Spinlock contended: burns 100% CPU for duration of wait\n");

    /* === EXERCISE ===
     * Try these hands-on tasks:
     * 1. Benchmark mutex vs spinlock for VERY SHORT critical sections (just counter++):
     *    Run with T=1 thread (no contention) and T=8 threads (high contention).
     *    At T=1: spinlock and mutex should be similar.
     *    At T=8: spinlock may be faster for short CS but will burn more CPU.
     *    Measure with: perf stat -e cpu-cycles,context-switches ./lab_19
     * 2. Demonstrate deadlock: create two mutexes and two threads that acquire them
     *    in opposite order.  Add a sleep between acquisitions to make deadlock reliable.
     *    Use pthread_mutex_trylock() in a loop to detect and break the deadlock.
     * 3. Modern (futex-based mutexes in glibc): use strace to observe futex syscalls:
     *      strace -e futex ./lab_19 2>&1 | head -50
     *    Under no contention: no futex calls (fast path entirely in userspace).
     *    Under high contention: many FUTEX_WAIT/FUTEX_WAKE pairs visible.
     *    This demonstrates the adaptive behavior of pthread_mutex.
     *
     * OBSERVE: Spinlock contended on 4 threads wastes 3 CPU-core equivalents of energy.
     *          On a 128-core server with many spinlocks, this can waste significant power.
     *          This is why Linux kernel moved many spinlocks to mutexes/RCU where possible.
     * WHY:     The kernel cannot use sleeping mutexes in interrupt context because
     *          interrupts run with the current thread's kernel stack and cannot block.
     *          Spinlocks disable preemption (and often IRQs) during the critical section,
     *          guaranteeing the lock holder runs to completion without being interrupted.
     */
    printf("\n========== Hands-On Exercise ==========\n");
    printf("1. Benchmark mutex vs spinlock at T=1 and T=8; measure CPU cycles and\n");
    printf("   context switches with perf stat to compare contention behavior.\n");
    printf("2. Demonstrate deadlock with two mutexes acquired in opposite order;\n");
    printf("   use pthread_mutex_trylock() loop to detect and recover.\n");
    printf("3. Modern (futex internals): use strace -e futex to observe zero futex\n");
    printf("   syscalls under no contention and FUTEX_WAIT/WAKE under contention.\n");

    printf("\n========== Quiz ==========\n");
    printf("Q1. When should you use a spinlock vs a mutex?\n");
    printf("Q2. What is a futex and how does pthread_mutex use it?\n");
    printf("Q3. Why can the kernel not use mutexes in interrupt handlers?\n");
    printf("Q4. What is priority inversion and how can it occur with mutexes?\n");
    printf("Q5. What is the uncontended fast path for pthread_mutex on Linux?\n");
    printf("    At what point does it fall back to a futex syscall?\n");
    printf("Q6. What is priority inheritance (PTHREAD_PRIO_INHERIT) and which\n");
    printf("    real-time systems require it (hint: Mars Pathfinder story)?\n");
    printf("Q7. Why does the Linux kernel use spinlocks that disable IRQs (spin_lock_irqsave)\n");
    printf("    in some cases, and what problem does IRQ disabling solve?\n");
    return 0;
}
