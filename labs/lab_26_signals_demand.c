/*
 * lab_26_signals_demand.c
 * Topic: Signals, IDE Driver, Intro Demand Paging
 * Build: gcc -O0 -Wall -pthread -o lab_26 lab_26_signals_demand.c
 *
 * Author: Arun Singh | arunsingh.in@gmail.com
 * OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/resource.h>
static void ps(const char *t){printf("\n========== %s ==========\n",t);}
static volatile sig_atomic_t got_signal=0;
static void handler(int sig){got_signal=sig;}
int main(void){
    printf("=== Lab 26: Signals, Device Drivers, Intro to Demand Paging ===\n");
    ps("Phase 1: Signal Delivery");
    signal(SIGUSR1,handler);
    printf("  Sending SIGUSR1 to self (PID %d)...\n",getpid());
    kill(getpid(),SIGUSR1);
    printf("  Handler received signal: %d (%s)\n",got_signal,got_signal==SIGUSR1?"SIGUSR1":"?");
    printf("  Signals are software interrupts delivered by the kernel.\n");
    printf("  Async signals can interrupt any instruction — handler must be reentrant.\n");
    ps("Phase 2: Block Device Concepts");
    printf("  Traditional IDE/SATA driver flow:\n");
    printf("  1. Process calls read() -> VFS -> filesystem -> block layer\n");
    printf("  2. Block layer issues I/O request to device driver\n");
    printf("  3. Driver programs DMA controller, process sleeps\n");
    printf("  4. Device completes I/O, raises interrupt (IRQ)\n");
    printf("  5. Interrupt handler wakes process, data is in page cache\n\n");
    printf("  Modern: NVMe uses polled I/O + MSI-X interrupts + io_uring.\n");
    printf("  GPU DCs: NVMe SSDs for checkpointing, GDS (GPU Direct Storage) bypasses CPU.\n");
    ps("Phase 3: Demand Paging Intro");
    struct rusage ru;getrusage(RUSAGE_SELF,&ru);long f0=ru.ru_minflt;
    void *p=mmap(NULL,4096*100,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    getrusage(RUSAGE_SELF,&ru);long f1=ru.ru_minflt;
    printf("  mmap 100 pages: faults=%ld (just metadata)\n",f1-f0);
    memset(p,0xAB,4096*100);
    getrusage(RUSAGE_SELF,&ru);long f2=ru.ru_minflt;
    printf("  touch 100 pages: faults=%ld (demand paging!)\n",f2-f1);
    munmap(p,4096*100);
    printf("\n========== Quiz ==========\n");
    printf("Q1. What makes a signal handler 'reentrant-safe'?\n");
    printf("Q2. Trace a read() from userspace to disk and back.\n");
    printf("Q3. What is GPU Direct Storage and why does it matter for AI training checkpoints?\n");
    printf("Q4. Why does mmap not cause page faults but memset does?\n");
    return 0;}
