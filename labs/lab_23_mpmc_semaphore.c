/*
 * lab_23_mpmc_semaphore.c
 * Topic: MPMC, Semaphores, Monitors
 * Build: gcc -O0 -Wall -pthread -o lab_23 lab_23_mpmc_semaphore.c
 *
 * Author: Arun Singh | arunsingh.in@gmail.com
 * OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
*/
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
static void ps(const char *t){printf("\n========== %s ==========\n",t);}
#define BUFSIZE 5
static int buffer[BUFSIZE];
static sem_t empty_slots,full_slots;
static pthread_mutex_t buf_mtx=PTHREAD_MUTEX_INITIALIZER;
static int in_i=0,out_i=0,total_produced=0,total_consumed=0;
static volatile int stop=0;
static void *producer(void*arg){
    int id=*(int*)arg;
    for(int i=0;i<10&&!stop;i++){
        sem_wait(&empty_slots);
        pthread_mutex_lock(&buf_mtx);
        int val=__sync_fetch_and_add(&total_produced,1);
        buffer[in_i]=val;in_i=(in_i+1)%BUFSIZE;
        printf("  P%d: put %d\n",id,val);
        pthread_mutex_unlock(&buf_mtx);
        sem_post(&full_slots);
        usleep(5000);
    }return NULL;}
static void *consumer(void*arg){
    int id=*(int*)arg;
    for(int i=0;i<10&&!stop;i++){
        sem_wait(&full_slots);
        pthread_mutex_lock(&buf_mtx);
        int val=buffer[out_i];out_i=(out_i+1)%BUFSIZE;
        __sync_fetch_and_add(&total_consumed,1);
        printf("  C%d: got %d\n",id,val);
        pthread_mutex_unlock(&buf_mtx);
        sem_post(&empty_slots);
        usleep(8000);
    }return NULL;}
int main(void){
    printf("=== Lab 23: MPMC Queue, Semaphores ===\n");
    ps("Multi-Producer Multi-Consumer with Semaphores");
    sem_init(&empty_slots,0,BUFSIZE);
    sem_init(&full_slots,0,0);
    int ids[]={0,1,2,3};
    pthread_t p[2],c[2];
    for(int i=0;i<2;i++){pthread_create(&p[i],NULL,producer,&ids[i]);pthread_create(&c[i],NULL,consumer,&ids[i+2]);}
    for(int i=0;i<2;i++){pthread_join(p[i],NULL);pthread_join(c[i],NULL);}
    printf("\n  Total produced: %d, consumed: %d\n",total_produced,total_consumed);
    printf("\n  OBSERVE: Semaphores count available resources (slots/items).\n");
    printf("  sem_wait decrements (blocks at 0), sem_post increments.\n");
    printf("  This is the classic bounded buffer / monitor pattern.\n");

    /* WHY: io_uring (Linux 5.1+) uses a pair of shared memory rings (similar to this
     *      semaphore-based bounded buffer) for async I/O:
     *      - SQ (Submission Queue): userspace writes SQEs (I/O requests), kernel reads.
     *      - CQ (Completion Queue): kernel writes CQEs (I/O results), userspace reads.
     *      Both rings are in shared memory (mmap'd from io_uring fd).
     *      Producers (userspace for SQ, kernel for CQ) update the tail pointer.
     *      Consumers (kernel for SQ, userspace for CQ) update the head pointer.
     *      The semaphore role is played by atomic ring pointers + io_uring_enter().
     *      This eliminates per-I/O syscall overhead: batch 1000 I/Os, one syscall.
     *
     * WHY: POSIX named semaphores (sem_open) allow semaphores to be shared between
     *      processes (backed by /dev/shm or similar).  Anonymous semaphores (sem_init
     *      with pshared=1) require shared memory (mmap) between processes.
     *      Semaphore vs mutex: a semaphore can be signaled from a DIFFERENT thread
     *      than the one that waited -- enabling producer/consumer patterns.
     *      A mutex must be unlocked by the same thread that locked it.
     */
    ps("Phase 2: io_uring Submission/Completion Ring Analogy");
    printf("  io_uring uses two lock-free rings in shared memory:\n");
    printf("    SQ ring: userspace -> kernel (I/O request submission)\n");
    printf("    CQ ring: kernel -> userspace (I/O completion notification)\n\n");
    printf("  Ring structure (conceptually like our bounded buffer):\n");
    printf("    - head: consumer reads from here\n");
    printf("    - tail: producer writes here\n");
    printf("    - Entries available: (tail - head) & ring_mask\n");
    printf("    - Empty: head == tail\n");
    printf("    - Full: (tail - head) == ring_size\n\n");
    printf("  io_uring advantages over this semaphore-based approach:\n");
    printf("    1. Lock-free: uses atomic ring pointers (no mutex needed)\n");
    printf("    2. Batch submission: fill 1000 SQEs, one io_uring_enter() syscall\n");
    printf("    3. Kernel polling mode (IORING_SETUP_SQPOLL): zero syscalls per I/O\n");
    printf("  Used by: PostgreSQL, RocksDB, NGINX, Glommio (async Rust runtime).\n");

    /* === EXERCISE ===
     * Try these hands-on tasks:
     * 1. Implement an MPMC queue using only semaphores (no mutex):
     *    This requires using 2 semaphores + 1 atomic index (CAS) for the slot.
     *    Verify correctness: total_produced == total_consumed for all runs.
     * 2. Extend to named semaphores for inter-process communication:
     *      sem_open("/lab23_full", O_CREAT, 0644, 0)
     *    Fork a child producer and parent consumer; use named semaphores to coordinate.
     *    Clean up: sem_unlink("/lab23_full") after use.
     * 3. Modern (io_uring): implement a simple file-read loop using io_uring:
     *      int ring_fd = syscall(__NR_io_uring_setup, QUEUE_DEPTH, &params);
     *    Submit multiple pread requests in one SQ batch, then wait for CQEs.
     *    Measure: ops/sec with io_uring vs blocking pread() for 10K small reads.
     *
     * OBSERVE: Semaphore-based MPMC still requires a mutex for the buffer index update.
     *          This is a classic 2-semaphore + 1-mutex pattern.  The semaphores handle
     *          the count (how many items/slots), the mutex handles the index update.
     *          io_uring avoids the mutex by using power-of-2 ring sizes and atomic
     *          head/tail pointers with careful memory ordering.
     * WHY:     sem_wait on Linux uses futex(FUTEX_WAIT) internally, same as pthread_mutex.
     *          The difference: a semaphore tracks a COUNT, a mutex tracks OWNERSHIP.
     *          sem_post can be called from a signal handler; pthread_mutex_unlock cannot.
     */
    printf("\n========== Hands-On Exercise ==========\n");
    printf("1. Implement MPMC queue using only semaphores + atomic CAS for slot index.\n");
    printf("2. Extend to named semaphores (sem_open) for inter-process producer/consumer.\n");
    printf("3. Modern (io_uring): implement pread batching with io_uring SQ/CQ rings;\n");
    printf("   compare ops/sec vs blocking pread() for 10K small reads.\n");

    printf("\n========== Quiz ==========\n");
    printf("Q1. How does a semaphore differ from a mutex?\n");
    printf("Q2. What is a monitor and how do condvar+mutex approximate one?\n");
    printf("Q3. Why do we need both semaphores AND a mutex in this MPMC queue?\n");
    printf("Q4. Can semaphores be used across processes? How?\n");
    printf("Q5. Explain the io_uring SQ/CQ ring design.  How does it avoid per-I/O syscalls?\n");
    printf("Q6. What is the difference between sem_wait and futex(FUTEX_WAIT) in the kernel?\n");
    printf("    When would you use one over the other?\n");
    printf("Q7. Why can sem_post be called from a signal handler but pthread_mutex_unlock\n");
    printf("    cannot?  What POSIX requirement makes this possible?\n");
    return 0;
}
