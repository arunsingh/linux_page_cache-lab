/*
 * lab_16.c
 * Topic: Process kernel stack, scheduler, fork, PCB
 * Build: gcc -O0 -Wall -pthread -o lab_16 lab_16_*.c
 *
 * Author: Arun Singh | arunsingh.in@gmail.com
 * OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sched.h>
#include <sys/resource.h>
static void print_section(const char *t){printf("\n========== %s ==========\n",t);}
int main(void){
    printf("=== Lab 16: Kernel Stack, Scheduler, Fork, PCB ===\n");
    print_section("Phase 1: Scheduler Info from /proc");
    char path[128];
    snprintf(path,sizeof(path),"/proc/%d/sched",getpid());
    FILE *fp=fopen(path,"r");
    if(fp){
        char l[256];int n=0;
        while(fgets(l,sizeof(l),fp)&&n++<15)printf("  %s",l);
        fclose(fp);
    }
    print_section("Phase 2: Scheduling Policy");
    int policy=sched_getscheduler(0);
    printf("  Current policy: %s\n",
        policy==SCHED_OTHER?"SCHED_OTHER (CFS)":
        policy==SCHED_FIFO?"SCHED_FIFO":
        policy==SCHED_RR?"SCHED_RR":"UNKNOWN");
    printf("  Nice value: %d\n",getpriority(PRIO_PROCESS,0));
    printf("  CFS (Completely Fair Scheduler) is default for normal processes.\n");
    printf("  GPU DC workloads may use SCHED_FIFO for latency-sensitive NCCL threads.\n");
    print_section("Phase 3: Kernel Stack Size");
    printf("  Default kernel stack: 8 KiB (x86, THREAD_SIZE) or 16 KiB (some configs)\n");
    printf("  This is much smaller than user stack (typically 8 MiB).\n");
    printf("  Kernel stack overflow = kernel panic (no guard page in traditional design).\n");
    printf("  Kernel 6.x: CONFIG_VMAP_STACK adds guard pages to kernel stacks.\n");
    print_section("Phase 4: PCB (task_struct) Fields");
    printf("  key fields in task_struct:\n");
    printf("    pid, tgid (thread group ID = process PID)\n");
    printf("    state (TASK_RUNNING, TASK_INTERRUPTIBLE, etc.)\n");
    printf("    mm (pointer to mm_struct, NULL for kernel threads)\n");
    printf("    fs (filesystem info), files (open file table)\n");
    printf("    signal (signal handling), stack (kernel stack pointer)\n");
    printf("    sched_entity (CFS scheduling)\n");
    printf("    cpus_mask (CPU affinity)\n");
    printf("\n========== Quiz ==========\n");
    printf("Q1. What is the difference between PID and TGID in the kernel?\n");
    printf("Q2. Why is the kernel stack so much smaller than user stack?\n");
    printf("Q3. What does CFS guarantee that SCHED_FIFO does not?\n");
    printf("Q4. What happens if a kernel function overflows the kernel stack?\n");
    return 0;
}
