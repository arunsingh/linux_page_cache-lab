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
    /* WHY: C11 atomics vs Linux kernel memory barriers:
     *      C11 _Atomic with memory_order_seq_cst compiles to heavy barriers on POWER/ARM
     *      (mfence on x86, dmb ish on ARM64).  The kernel avoids SC atomics and uses
     *      explicit smp_mb() / smp_rmb() / smp_wmb() which compile to the MINIMUM
     *      barrier needed for the architecture's memory model.
     *      On x86 (TSO): smp_rmb() and smp_wmb() are no-ops (compiler barriers only).
     *      On ARM64 (weakly ordered): smp_rmb() = dmb ishld, smp_wmb() = dmb ishst.
     *      This makes kernel code maximally efficient on each architecture.
     *
     * WHY: The acquire/release idiom implements a "happens-before" edge:
     *      Everything done before release-store is visible after acquire-load.
     *      This is the minimal synchronization needed for lock/unlock semantics.
     *      pthread_mutex_lock uses acquire; pthread_mutex_unlock uses release.
     *      Using seq_cst unnecessarily is a common performance bug in lock-free code.
     */
    ps("Phase 4: Acquire/Release vs Sequential Consistency Benchmark");
    {
        /* Demonstrate acquire/release pattern */
        atomic_int lock_flag = ATOMIC_VAR_INIT(0);
        long protected_data = 0;

        /* Publisher: write data with release semantics */
        protected_data = 12345;
        atomic_store_explicit(&lock_flag, 1, memory_order_release);

        /* Subscriber: load with acquire semantics */
        if (atomic_load_explicit(&lock_flag, memory_order_acquire)) {
            printf("  acquire/release: data=%ld  (release-store guarantees visibility)\n",
                   protected_data);
        }

        /* Compare: seq_cst version (same result, more expensive on ARM) */
        atomic_store(&lock_flag, 0); /* reset */
        protected_data = 99999;
        atomic_store(&lock_flag, 1); /* seq_cst (default) */
        if (atomic_load(&lock_flag)) {
            printf("  seq_cst:          data=%ld  (stronger guarantee, higher cost on ARM)\n",
                   protected_data);
        }

        printf("\n  On x86 (TSO): both produce identical code (x86 already seq_cst for loads/stores)\n");
        printf("  On ARM64: release=stlr, acquire=ldar; seq_cst adds dmb ish (full fence)\n");
    }

    /* === EXERCISE ===
     * Try these hands-on tasks:
     * 1. Implement a custom spinlock using C11 atomics with acquire/release:
     *      void spin_lock(atomic_flag *f) {
     *          while (atomic_flag_test_and_set_explicit(f, memory_order_acquire));
     *      }
     *      void spin_unlock(atomic_flag *f) {
     *          atomic_flag_clear_explicit(f, memory_order_release);
     *      }
     *    Benchmark against pthread_spinlock for N=1M iterations with 4 threads.
     * 2. Implement sleep/wakeup using futex directly (like the lab demo).
     *    Extend it to a semaphore: counter stored in atomic_int, futex used for wait.
     *    Test: producer increments semaphore, consumer decrements/waits.
     * 3. Modern (C11 atomics vs kernel memory barriers): compile a small C program
     *    with -mcpu=cortex-a72 and inspect the assembly for:
     *      atomic_store(p, v)                          (no barrier)
     *      atomic_store_explicit(p, v, memory_order_release)   (stlr)
     *      atomic_store_explicit(p, v, memory_order_seq_cst)   (dmb ish + str + dmb ish)
     *    Observe the growing cost of stronger memory ordering on ARM64.
     *
     * OBSERVE: FUTEX_WAIT returns EAGAIN if the value at addr != expected at the time
     *          of the syscall.  The while(val==0) futex_wait(...) loop handles this:
     *          if the value changed between the check and the syscall, EAGAIN fires
     *          and the loop re-checks -- preventing the "lost wakeup" for futexes.
     * WHY:     The C11 memory model was introduced because compilers and CPUs can
     *          reorder memory operations for performance.  Without explicit ordering,
     *          lock-free code is undefined behavior in C, even if it "works" on x86.
     *          The kernel predates C11 and uses its own barriers for portability.
     */
    printf("\n========== Hands-On Exercise ==========\n");
    printf("1. Implement custom spinlock with atomic_flag acquire/release;\n");
    printf("   benchmark vs pthread_spinlock for 4-thread contention.\n");
    printf("2. Implement futex-based semaphore: counter + futex_wait/wake;\n");
    printf("   test with producer/consumer pattern.\n");
    printf("3. Modern (C11 atomics on ARM64): compare asm output for relaxed vs\n");
    printf("   release vs seq_cst stores; observe growing barrier overhead.\n");

    printf("\n========== Quiz ==========\n");
    printf("Q1. Why is futex faster than a pure kernel mutex for uncontended locks?\n");
    printf("Q2. What is the 'lost wakeup' problem and how is it prevented?\n");
    printf("Q3. Explain acquire/release semantics with a producer-consumer example.\n");
    printf("Q4. How does the kernel wait_event() macro prevent lost wakeups?\n");
    printf("Q5. What is the difference between memory_order_acquire/release and\n");
    printf("    memory_order_seq_cst in terms of generated CPU instructions on ARM64?\n");
    printf("Q6. Why does the Linux kernel use smp_mb() instead of C11 _Atomic with\n");
    printf("    memory_order_seq_cst for shared variables?\n");
    printf("Q7. Explain the FUTEX_WAIT + FUTEX_WAKE handoff protocol.  What does\n");
    printf("    FUTEX_WAIT return if the value changed before the syscall executed?\n");
    return 0;
}
