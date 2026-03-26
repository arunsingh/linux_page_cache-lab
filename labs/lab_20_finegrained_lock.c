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

    /* WHY: RCU (Read-Copy-Update) in Linux networking:
     *      The Linux routing table (FIB -- Forwarding Information Base) uses RCU.
     *      Readers (packet forwarding) hold an RCU read-side lock (rcu_read_lock())
     *      which is essentially free (disables preemption, no memory barrier on TSO).
     *      Writers (routing table updates) create a new copy of the data, update it,
     *      then atomically replace the old pointer with the new one.
     *      After the old pointer is replaced, writers wait for a "grace period"
     *      (all pre-existing readers to finish) before freeing the old data.
     *      This allows millions of concurrent readers with zero lock overhead.
     *      RCU is used in: routing tables, network device lists, task list, VFS dcache.
     *
     * WHY: False sharing: when two unrelated variables share the same cache line (64B),
     *      writes to one invalidate the cache line for the other on all CPUs.
     *      The per-bucket-lock array should be padded to avoid false sharing.
     *      bucket_locks[0] and bucket_locks[1] may share a cache line (pthread_mutex_t
     *      is 40 bytes, 2 fit in 80 bytes = 2 cache lines, but 3 fit in 120 bytes, etc.)
     *      In production hash tables, use __attribute__((aligned(64))) for each lock.
     */
    print_section("Phase 3: Hash Table with Per-Bucket Locks");
    printf("  This lab already implements per-bucket locking (64 buckets, 64 locks).\n\n");
    printf("  Production hash table improvements:\n");
    printf("    1. Align each lock to cache line size (64B) to prevent false sharing.\n");
    printf("    2. Use RCU for read-heavy tables (e.g., Linux routing table).\n");
    printf("    3. Use lock striping: fewer locks than buckets (lock[bucket %% N_LOCKS]).\n\n");
    printf("  False sharing check:\n");
    printf("    sizeof(pthread_mutex_t)=%zu bytes per lock\n", sizeof(pthread_mutex_t));
    printf("    %d locks * %zu bytes = %zu bytes (%zu cache lines)\n",
           NBUCKETS, sizeof(pthread_mutex_t),
           NBUCKETS * sizeof(pthread_mutex_t),
           (NBUCKETS * sizeof(pthread_mutex_t) + 63) / 64);
    if (sizeof(pthread_mutex_t) < 64)
        printf("    WARNING: locks smaller than cache line -- false sharing possible!\n");
    else
        printf("    OK: each lock >= 1 cache line (no false sharing).\n");

    printf("\n  RCU (Read-Copy-Update) concept:\n");
    printf("    Readers: rcu_read_lock() / rcu_read_unlock() (disable preemption, ~0 cost)\n");
    printf("    Writers: copy -> modify -> rcu_assign_pointer() -> synchronize_rcu()\n");
    printf("    Used in: Linux routing (FIB), dcache, task list, network device list.\n");
    printf("    Benefit: readers never block, even during concurrent writes.\n");

    /* === EXERCISE ===
     * Try these hands-on tasks:
     * 1. Implement a 1024-bucket hash table with per-bucket locks.  Use 4 threads
     *    each inserting 500K items.  Compare throughput vs a single global lock.
     *    Then add padding: struct { pthread_mutex_t lock; char pad[64-sizeof(pthread_mutex_t)]; }
     *    to eliminate false sharing.  Measure the speedup from padding.
     * 2. Add a read-heavy workload: 90% lookups, 10% insertions.  Measure
     *    throughput with per-bucket mutex vs per-bucket rwlock (pthread_rwlock_t).
     *    For 8 reader threads: rwlock should scale better than mutex.
     * 3. Modern (RCU in Linux networking): study the Linux kernel's fib_table_lookup()
     *    in net/ipv4/fib_trie.c.  It uses rcu_read_lock() for the entire lookup --
     *    no mutex, no spinlock.  The writer (route add/delete) uses call_rcu() for
     *    deferred free.  This enables millions of lookups/second per core.
     *
     * OBSERVE: The per-bucket-lock version is faster because 4 threads can operate
     *          on 4 different buckets simultaneously.  The global lock serializes all 4.
     *          Speedup = min(T, N_BUCKETS) for perfectly uniform hash distribution.
     * WHY:     Lock granularity vs deadlock risk: with N locks, there are O(N^2) possible
     *          lock orderings.  Always acquire in bucket-index order to prevent deadlock
     *          (e.g., when moving an item between buckets, always lock lower index first).
     */
    printf("\n========== Hands-On Exercise ==========\n");
    printf("1. Implement 1024-bucket hash table; compare global vs per-bucket lock;\n");
    printf("   add 64B padding to locks and measure false-sharing elimination speedup.\n");
    printf("2. Add 90%% reads / 10%% writes workload; compare mutex vs rwlock throughput.\n");
    printf("3. Modern (RCU): study fib_table_lookup() in Linux net/ipv4/fib_trie.c;\n");
    printf("   trace the rcu_read_lock/unlock pattern and call_rcu deferred free.\n");

    printf("\n========== Quiz ==========\n");
    printf("Q1. Why is fine-grained locking faster under contention?\n");
    printf("Q2. What is lock ordering and why does it prevent deadlock?\n");
    printf("Q3. Give a kernel example of fine-grained locking.\n");
    printf("Q4. What is the tradeoff between lock granularity and complexity?\n");
    printf("Q5. What is false sharing and how does it affect per-element locking in\n");
    printf("    a hash table?  How do you fix it with struct padding?\n");
    printf("Q6. How does RCU (Read-Copy-Update) achieve zero overhead for readers?\n");
    printf("    What is a 'grace period' and why must writers wait for it?\n");
    printf("Q7. In the Linux routing table (FIB), why is RCU preferred over per-bucket\n");
    printf("    mutexes for the forwarding path in datacenter routers?\n");
    return 0;
}
