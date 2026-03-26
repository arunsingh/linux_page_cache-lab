/*
 * lab_18.c
 * Topic: Handling user pointers, concurrency
 * Build: gcc -O0 -Wall -pthread -o lab_18 lab_18_*.c
 *
 * Author: Arun Singh | arunsingh.in@gmail.com
 * OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdatomic.h>
#include <unistd.h>
static void print_section(const char *t){printf("\n========== %s ==========\n",t);}
static volatile long counter_racy=0;
static atomic_long counter_safe=0;
static void *racy_inc(void *arg){
    int n=*(int*)arg;
    for(int i=0;i<n;i++)counter_racy++;
    return NULL;
}
static void *safe_inc(void *arg){
    int n=*(int*)arg;
    for(int i=0;i<n;i++)atomic_fetch_add(&counter_safe,1);
    return NULL;
}
int main(void){
    printf("=== Lab 18: User Pointers, Concurrency ===\n");
    print_section("Phase 1: Data Race Demonstration");
    int N=1000000,T=4;
    pthread_t threads[4];int args[4];
    for(int i=0;i<T;i++){args[i]=N;pthread_create(&threads[i],NULL,racy_inc,&args[i]);}
    for(int i=0;i<T;i++)pthread_join(threads[i],NULL);
    printf("  Expected: %d, Got: %ld (LOST %ld updates!)\n",N*T,counter_racy,(long)N*T-counter_racy);
    print_section("Phase 2: Atomic Fix");
    for(int i=0;i<T;i++){args[i]=N;pthread_create(&threads[i],NULL,safe_inc,&args[i]);}
    for(int i=0;i<T;i++)pthread_join(threads[i],NULL);
    printf("  Expected: %d, Got: %ld (correct with atomics)\n",N*T,atomic_load(&counter_safe));
    print_section("Phase 3: User Pointer Safety in Kernel");
    printf("  When kernel handles syscalls, user pointers must be validated:\n");
    printf("    copy_from_user() / copy_to_user() — checks address range + handles faults\n");
    printf("    access_ok() — verifies pointer is in user address space\n");
    printf("  Without these checks: user could trick kernel into reading/writing kernel memory.\n");
    printf("  This was the basis of many privilege escalation exploits.\n");
    /* WHY: Linux kernel memory model (LKMM) is documented in tools/memory-model/
     *      It defines the allowed reorderings for READ_ONCE/WRITE_ONCE, smp_mb(),
     *      smp_rmb(), smp_wmb(), and atomic operations.  Key rules:
     *      - READ_ONCE(x): prevents compiler from merging/optimizing the read
     *      - WRITE_ONCE(x,v): prevents compiler from splitting/reordering the write
     *      - smp_mb(): full memory barrier -- no loads/stores cross in either direction
     *      - smp_rmb(): read-side barrier (all reads before are complete before next)
     *      - smp_wmb(): write-side barrier (all writes before are visible before next)
     *      C11 _Atomic is NOT the same: it uses SC (sequentially consistent) by default,
     *      which is stronger than needed and has higher overhead on POWER/ARM architectures.
     *
     * WHY: Data races are undefined behavior in C11.  The compiler can hoist loads out
     *      of loops, merge duplicate stores, or assume a non-volatile variable does not
     *      change between two reads.  This is why the kernel uses READ_ONCE/WRITE_ONCE
     *      for shared variables even under a lock (to prevent compiler reordering).
     */
    print_section("Phase 4: Memory Barriers and smp_mb() Concept");
    printf("  C11 memory model provides:\n");
    printf("    memory_order_relaxed:  no ordering guarantee (fastest)\n");
    printf("    memory_order_acquire:  no reads/writes moved before this load\n");
    printf("    memory_order_release:  no reads/writes moved after this store\n");
    printf("    memory_order_seq_cst:  full barrier (default for _Atomic, expensive)\n\n");
    {
        /* Demonstrate acquire/release pattern for lock-free flag */
        atomic_int flag = ATOMIC_VAR_INIT(0);
        volatile long shared_data = 0;

        /* Simulate producer: write data, then set flag with release */
        shared_data = 42;
        atomic_store_explicit(&flag, 1, memory_order_release);

        /* Simulate consumer: check flag with acquire, then read data */
        if (atomic_load_explicit(&flag, memory_order_acquire)) {
            printf("  Acquire/release: flag=1, shared_data=%ld (guaranteed visible)\n",
                   shared_data);
            printf("  WHY: release-store ensures 'shared_data=42' is visible before flag=1.\n");
            printf("       acquire-load ensures 'shared_data' read sees the released write.\n");
        }
    }
    printf("\n  Linux kernel equivalent:\n");
    printf("    smp_store_release(&flag, 1);  // release semantics\n");
    printf("    v = smp_load_acquire(&flag);  // acquire semantics\n");
    printf("    smp_mb();                     // full barrier (expensive, use sparingly)\n");

    /* === EXERCISE ===
     * Try these hands-on tasks:
     * 1. Demonstrate a data race WITH and WITHOUT mutex:
     *    Run the racy_inc version 10 times and record the lost-update count.
     *    Replace with a mutex-protected version and run 10 times -- all should be exact.
     *    Use ThreadSanitizer to detect the race automatically:
     *      gcc -fsanitize=thread -o lab_18_tsan lab_18_concurrency.c -pthread
     *      ./lab_18_tsan
     *    TSAN will report the exact race location and the two conflicting threads.
     * 2. Benchmark atomic_fetch_add vs mutex-protected increment for N=10M iterations.
     *    Measure throughput with perf or clock_gettime.  The lock-free atomic should
     *    be ~5-10x faster for this trivial critical section.
     * 3. Modern (smp_mb() in Linux kernel): study the classic Peterson's algorithm and
     *    show that it requires memory barriers to be correct on weakly-ordered CPUs (ARM).
     *    C11 atomics with memory_order_seq_cst provide the necessary ordering.
     *    Compare: on x86 TSO model, Peterson's works without explicit barriers (TSO
     *    prevents StoreLoad reordering only).  On ARM/POWER, explicit barriers needed.
     *
     * OBSERVE: The racy counter loses updates because counter_racy++ compiles to
     *          three instructions: LOAD, ADD, STORE.  Two threads can interleave
     *          at the LOAD-ADD boundary, causing one update to be lost (last-write wins).
     * WHY:     atomic_fetch_add on x86 uses LOCK XADD, a single indivisible instruction.
     *          The LOCK prefix asserts the bus/cache-line for the duration, preventing
     *          any other core from reading/writing that cache line between load and store.
     *          On ARM, it uses LDADD (Armv8.1 LSE) or LDXR/STXR loop (Armv8.0).
     */
    printf("\n========== Hands-On Exercise ==========\n");
    printf("1. Run racy_inc 10x; record lost updates. Use -fsanitize=thread to detect race.\n");
    printf("2. Benchmark atomic vs mutex increment for N=10M; compare throughput.\n");
    printf("3. Modern (smp_mb/memory ordering): implement Peterson's algorithm with C11\n");
    printf("   atomics; test on ARM vs x86 to observe memory model differences.\n");

    printf("\n========== Quiz ==========\n");
    printf("Q1. Why did the racy counter lose updates? Describe the interleaving.\n");
    printf("Q2. What does atomic_fetch_add compile down to on x86? (hint: LOCK XADD)\n");
    printf("Q3. Why must the kernel validate user pointers before dereferencing them?\n");
    printf("Q4. What is SMAP/SMEP and how do they protect against user pointer attacks?\n");
    printf("Q5. What is the difference between memory_order_acquire/release and\n");
    printf("    memory_order_seq_cst? When would you use each?\n");
    printf("Q6. Why does the Linux kernel use READ_ONCE/WRITE_ONCE instead of C11\n");
    printf("    _Atomic for shared variables in the kernel memory model?\n");
    printf("Q7. On an ARM CPU (weakly ordered), can Peterson's mutual exclusion algorithm\n");
    printf("    work without explicit memory barriers? Why or why not?\n");
    return 0;
}
