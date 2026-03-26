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
    /* WHY: Generation counter (simplified quiescent-state detection):
     *      In this userspace simulation, we use usleep(100ms) as a crude "grace period".
     *      The Linux kernel determines grace periods by tracking when each CPU has
     *      passed through a "quiescent state" -- any point where that CPU cannot be
     *      in an RCU read-side critical section:
     *        - Context switch (CPU is now in different task or idle)
     *        - CPU going idle (no task running)
     *        - User-mode execution (kernel RCU sections are not active in user mode)
     *      Once ALL CPUs have passed a quiescent state, the grace period is over.
     *      Tiny RCU (for embedded), Tree RCU (for large SMP), and SRCU (sleepable RCU)
     *      are different implementations in the Linux kernel.
     *
     * WHY: RCU in Linux networking stack:
     *      - fib_table_lookup(): IPv4 routing uses RCU, enabling millions of lookups/sec
     *      - neigh_lookup(): ARP table uses RCU
     *      - dev_get_by_name(): network interface list uses RCU
     *      - nf_hook_slow(): netfilter hooks use RCU for hot-path packet processing
     *      Each rcu_read_lock() is just preempt_disable() on non-preemptible kernels.
     *      Zero atomic operations, zero cache misses for the read lock.
     */
    ps("Phase 4: RCU with Generation Counters (Grace Period Simulation)");
    {
        /* Simulate RCU with a generation counter approach */
        atomic_uint gen_counter = ATOMIC_VAR_INIT(0);
        data_t *current_ptr = atomic_load(&global_data);

        printf("  Simulating RCU grace period with generation counters:\n");
        printf("  Current data: version=%d value=%d\n",
               current_ptr->version, current_ptr->value);

        /* Writer: increment generation (signals "update in progress") */
        unsigned old_gen = atomic_fetch_add(&gen_counter, 1);
        printf("  Writer: incremented gen_counter to %u (write in progress)\n", old_gen + 1);

        /* Readers should see either old or new data, never partial */
        printf("  All readers that started before gen=%u must complete before free(old)\n", old_gen + 1);

        /* Complete generation (all readers have passed the quiescent state) */
        atomic_fetch_add(&gen_counter, 1);
        printf("  Writer: gen_counter=%u (even=stable, write complete)\n",
               atomic_load(&gen_counter));
        printf("  Now safe to free old data (no reader can still hold it)\n\n");
    }

    printf("  RCU in Linux networking (read-side cost analysis):\n");
    printf("    rcu_read_lock():   preempt_disable() = 1 instruction on PREEMPT=n\n");
    printf("    fib_table_lookup():  full route lookup under RCU, zero lock overhead\n");
    printf("    rcu_read_unlock(): preempt_enable() + check for deferred work\n");
    printf("    Result: millions of route lookups/sec per core on a 100Gbps router\n");

    /* === EXERCISE ===
     * Try these hands-on tasks:
     * 1. Implement a userspace RCU-like linked list:
     *    - Readers traverse the list holding an "epoch" counter (acquire)
     *    - Writers: copy head node, modify copy, swap pointer (release)
     *    - Grace period: wait for all readers to move past the old epoch
     *    Verify: readers never see a partially-updated list.
     * 2. Measure the overhead of rcu_read_lock/unlock vs pthread_rwlock_rdlock:
     *    Implement both patterns and benchmark 10M read operations.
     *    The RCU pattern (just preempt_disable equivalent) should be ~10x faster.
     * 3. Modern (RCU in kernel networking): trace the Linux networking hot path:
     *      fib_lookup() -> fib_table_lookup() -> rcu_read_lock/unlock
     *    Use bpftrace to measure the time in rcu_read_lock critical section:
     *      bpftrace -e 'kprobe:fib_table_lookup { @start = nsecs; }
     *                   kretprobe:fib_table_lookup { @lat = hist(nsecs - @start); }'
     *    Typical: < 1 us for cached routes.
     *
     * OBSERVE: The grace period wait (usleep(100ms) here) is the writer's overhead.
     *          In the kernel, this is amortized: writers batch updates and share grace
     *          periods.  call_rcu() defers the free until after the grace period,
     *          allowing writers to proceed without waiting (async RCU).
     * WHY:     smp_rmb() is a no-op on x86 because TSO (Total Store Order) means:
     *          stores are seen by other cores in program order, and loads from other
     *          cores are seen in the order they were stored.  Read barriers (rmb)
     *          are only needed when you need to prevent load reordering, which x86
     *          already prevents in hardware for non-WC memory.
     */
    printf("\n========== Hands-On Exercise ==========\n");
    printf("1. Implement userspace RCU linked list with epoch counters;\n");
    printf("   verify readers never see partial updates during concurrent writes.\n");
    printf("2. Benchmark RCU read (preempt_disable equivalent) vs rwlock_rdlock\n");
    printf("   for 10M reads; expect ~10x speedup for RCU.\n");
    printf("3. Modern (kernel networking): use bpftrace to measure fib_table_lookup()\n");
    printf("   latency; correlate with rcu_read_lock/unlock overhead.\n");

    printf("\n========== Quiz ==========\n");
    printf("Q1. Why is RCU so fast for readers?\n");
    printf("Q2. What is a grace period and how does the kernel determine it?\n");
    printf("Q3. When is RCU better than rwlocks?\n");
    printf("Q4. Why does smp_rmb() do nothing on x86 but emit instructions on ARM?\n");
    printf("Q5. What is a 'quiescent state' in RCU terminology?  How does the kernel\n");
    printf("    detect that all CPUs have passed through one?\n");
    printf("Q6. What is the difference between synchronize_rcu() and call_rcu()?\n");
    printf("    When would a writer prefer one over the other?\n");
    printf("Q7. How does RCU in the Linux IPv4 routing path (fib_table_lookup) enable\n");
    printf("    millions of route lookups per second without any locking?\n");
    return 0;
}
