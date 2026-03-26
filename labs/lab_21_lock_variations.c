/*
 * lab_21_lock_variations.c
 * Topic: Locking Variations
 * Build: gcc -O0 -Wall -pthread -o lab_21 lab_21_lock_variations.c
 *
 * Author: Arun Singh | arunsingh.in@gmail.com
 * OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
*/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
static void ps(const char *t){printf("\n========== %s ==========\n",t);}
static double now(void){struct timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);return ts.tv_sec*1e9+ts.tv_nsec;}
static pthread_rwlock_t rwl=PTHREAD_RWLOCK_INITIALIZER;
static long shared_data=0;
static int ITERS=500000;
static void *reader(void*a){(void)a;long s=0;
    for(int i=0;i<ITERS;i++){pthread_rwlock_rdlock(&rwl);s+=shared_data;pthread_rwlock_unlock(&rwl);}
    return (void*)s;}
static void *writer(void*a){(void)a;
    for(int i=0;i<ITERS/10;i++){pthread_rwlock_wrlock(&rwl);shared_data++;pthread_rwlock_unlock(&rwl);}
    return NULL;}
int main(void){
    printf("=== Lab 21: Locking Variations ===\n");
    ps("Phase 1: Read-Write Lock");
    printf("  Multiple readers can hold the lock simultaneously.\n");
    printf("  Writers get exclusive access.\n\n");
    pthread_t r[4],w[2];
    double t0=now();
    for(int i=0;i<4;i++)pthread_create(&r[i],NULL,reader,NULL);
    for(int i=0;i<2;i++)pthread_create(&w[i],NULL,writer,NULL);
    for(int i=0;i<4;i++)pthread_join(r[i],NULL);
    for(int i=0;i<2;i++)pthread_join(w[i],NULL);
    printf("  RWLock time: %.1f ms, shared_data=%ld\n",(now()-t0)/1e6,shared_data);
    ps("Phase 2: Trylock (Non-blocking)");
    pthread_mutex_t m=PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock(&m);
    int r2=pthread_mutex_trylock(&m);
    printf("  trylock while held: %s (returns EBUSY=%d)\n",r2?"FAILED":"OK",r2);
    pthread_mutex_unlock(&m);
    r2=pthread_mutex_trylock(&m);
    printf("  trylock while free: %s\n",r2?"FAILED":"OK");
    if(!r2)pthread_mutex_unlock(&m);
    ps("Phase 3: Recursive Mutex");
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr,PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_t rm;
    pthread_mutex_init(&rm,&attr);
    pthread_mutex_lock(&rm);
    pthread_mutex_lock(&rm);
    printf("  Recursive mutex: locked twice without deadlock.\n");
    pthread_mutex_unlock(&rm);pthread_mutex_unlock(&rm);
    /* WHY: seqlock (sequence lock) in kernel timekeeping:
     *      A seqlock allows concurrent readers without blocking writers.
     *      The writer increments a sequence counter (odd=updating, even=stable).
     *      Readers check the counter before and after reading the data.
     *      If the counter changed (or is odd), the reader retries.
     *      This is used for jiffies, xtime (wall clock), and HPET timestamps.
     *      seqlocks are read-heavy and write-rare: clock reads happen millions/sec.
     *      Linux kernel: read_seqbegin / read_seqretry / write_seqlock / write_sequnlock
     *
     * WHY: Writer starvation in rwlock: if new readers keep arriving continuously,
     *      a writer may wait indefinitely.  POSIX pthread_rwlock doesn't guarantee
     *      writer preference.  Linux kernel's rw_semaphore uses a fair queue.
     *      Some implementations (e.g., boost::shared_mutex) offer writer preference mode.
     */
    ps("Phase 4: seqlock Concept (Kernel Timekeeping)");
    printf("  seqlock: optimistic concurrency for read-heavy, write-rare data.\n");
    printf("  Sequence counter: odd=writer active, even=stable.\n\n");
    {
        /* Simulate seqlock pattern in userspace */
        volatile unsigned seq = 0;
        volatile long timestamp_sec = 1700000000L;
        volatile long timestamp_nsec = 123456789L;

        /* Simulate write */
        seq++; /* seq becomes 1 (odd: updating) */
        timestamp_sec = 1700000001L;
        timestamp_nsec = 987654321L;
        seq++; /* seq becomes 2 (even: stable) */

        /* Simulate read with retry */
        long ts_sec, ts_nsec;
        unsigned s1, s2;
        int retries = 0;
        do {
            s1 = seq;
            if (s1 & 1) { retries++; continue; } /* writer active, retry */
            ts_sec = timestamp_sec;
            ts_nsec = timestamp_nsec;
            s2 = seq;
        } while (s1 != s2);

        printf("  Read: ts=%ld.%ld (retries=%d)\n", ts_sec, ts_nsec, retries);
        printf("  WHY: Kernel uses seqlock for jiffies/xtime (millions of reads/sec).\n");
        printf("       No reader lock = zero contention between readers and writer.\n");
    }

    /* === EXERCISE ===
     * Try these hands-on tasks:
     * 1. Implement a read-write lock that favors writers (write-preferring):
     *    Track waiting_writers count.  Readers block if waiting_writers > 0.
     *    This prevents writer starvation at the cost of potentially starving readers.
     *    Benchmark: compare reader throughput with reader-preferring vs writer-preferring.
     * 2. Show reader concurrency: create 8 reader threads and 1 writer thread.
     *    Add a printf inside the read-lock section showing concurrent reader count.
     *    Verify that multiple readers can hold the lock simultaneously.
     *    Then replace with a mutex: readers must serialize, showing the difference.
     * 3. Modern (seqlock in kernel timekeeping): study the Linux kernel's
     *    timekeeping_get_ns() function in kernel/time/timekeeping.c.
     *    It uses read_seqbegin/read_seqretry to read xtime without locking.
     *    Explain why this is faster than a mutex for clock_gettime() on every syscall.
     *
     * OBSERVE: RWlock readers can run in parallel as long as no writer is active.
     *          With 4 readers and 1 writer, the rwlock should be ~4x faster than
     *          a mutex for a read-heavy (90% read) workload.
     * WHY:     seqlock is appropriate when data fits in a few words and readers
     *          can afford to retry.  If the critical section is large (copying a
     *          full struct), seqlock retries are expensive and rwlock is better.
     */
    printf("\n========== Hands-On Exercise ==========\n");
    printf("1. Implement write-preferring rwlock; compare starvation behavior vs\n");
    printf("   standard pthread_rwlock under continuous reader load.\n");
    printf("2. Show reader concurrency: 8 readers simultaneously in rwlock;\n");
    printf("   replace with mutex and show readers must serialize.\n");
    printf("3. Modern (seqlock): study timekeeping_get_ns() in Linux kernel;\n");
    printf("   explain why seqlock is preferred over mutex for clock reads.\n");

    printf("\n========== Quiz ==========\n");
    printf("Q1. When is a rwlock better than a mutex? When is it worse?\n");
    printf("Q2. What problem does trylock solve? Give a real-world example.\n");
    printf("Q3. Why are recursive mutexes considered a code smell?\n");
    printf("Q4. What is writer starvation in rwlocks and how can it be prevented?\n");
    printf("Q5. Explain the seqlock algorithm: what does odd vs even sequence counter mean,\n");
    printf("    and why must the reader loop retry when seq changes?\n");
    printf("Q6. Why does the Linux kernel use seqlock for jiffies and xtime rather than\n");
    printf("    a spinlock, given that clock_gettime() is called millions of times/sec?\n");
    printf("Q7. What is the POSIX guarantee for pthread_rwlock_rdlock regarding\n");
    printf("    writer starvation, and how does this differ from Linux kernel rw_semaphore?\n");
    return 0;
}
