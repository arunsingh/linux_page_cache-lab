/*
 * lab_35_scheduling.c
 * Topic: Scheduling Policies
 * Build: gcc -O0 -Wall -pthread -o lab_35 lab_35_scheduling.c
 *
 * Author: Arun Singh | arunsingh.in@gmail.com
 * OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
*/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <sys/resource.h>
static void ps(const char *t){printf("\n========== %s ==========\n",t);}
int main(void){
    printf("=== Lab 35: Scheduling Policies ===\n");
    ps("Phase 1: CFS (Completely Fair Scheduler)");
    printf("  Default scheduler for SCHED_OTHER/SCHED_NORMAL tasks.\n");
    printf("  Uses virtual runtime (vruntime) to ensure fairness.\n");
    printf("  Stored in red-black tree ordered by vruntime.\n");
    printf("  Nice values -20 to +19 adjust weight (lower nice = more CPU).\n\n");
    printf("  Current nice: %d\n",getpriority(PRIO_PROCESS,0));
    printf("  Timeslice target: /proc/sys/kernel/sched_min_granularity_ns\n");
    FILE *fp=fopen("/proc/sys/kernel/sched_min_granularity_ns","r");
    if(fp){char l[64];if(fgets(l,sizeof(l),fp))printf("  sched_min_granularity: %s",l);fclose(fp);}
    ps("Phase 2: Real-Time Schedulers");
    printf("  SCHED_FIFO: first-in first-out, no timeslice, runs until yield/block.\n");
    printf("  SCHED_RR:   round-robin with fixed timeslice among same-priority tasks.\n");
    printf("  Priority: 1-99 (higher = more urgent). Always preempts CFS tasks.\n");
    printf("  GPU DCs: NCCL communication threads often use SCHED_FIFO.\n");
    ps("Phase 3: SCHED_DEADLINE");
    printf("  Earliest Deadline First (EDF) scheduling.\n");
    printf("  Parameters: runtime, deadline, period.\n");
    printf("  Guarantees: if runtime <= deadline, task will complete on time.\n");
    printf("  Used for: real-time audio/video, robotics, latency-critical GPU work.\n");
    ps("Phase 4: CPU Affinity");
    cpu_set_t mask;
    CPU_ZERO(&mask);
    sched_getaffinity(0,sizeof(mask),&mask);
    printf("  This process can run on CPUs:");
    for(int i=0;i<CPU_SETSIZE&&i<16;i++)if(CPU_ISSET(i,&mask))printf(" %d",i);
    printf("\n  GPU DCs: pin NCCL threads to specific NUMA-local CPUs.\n");
    printf("\n========== Quiz ==========\n");
    printf("Q1. How does CFS ensure fairness using vruntime?\n");
    printf("Q2. Why would you use SCHED_FIFO for GPU communication threads?\n");
    printf("Q3. What is the risk of using SCHED_FIFO without care? (hint: starvation)\n");
    printf("Q4. How does CPU affinity interact with NUMA topology?\n");
    return 0;}
