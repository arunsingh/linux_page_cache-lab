/*
 * lab_37_kernel_arch.c
 * Topic: Microkernel, Exokernel, Multikernel
 * Build: gcc -O0 -Wall -pthread -o lab_37 lab_37_kernel_arch.c
 *
 * Author: Arun Singh | arunsingh.in@gmail.com
 * OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
*/
#include <stdio.h>
static void ps(const char *t){printf("\n========== %s ==========\n",t);}
int main(void){
    printf("=== Lab 37: Microkernel, Exokernel, Multikernel ===\n");
    ps("Phase 1: Monolithic Kernel (Linux)");
    printf("  All services in kernel space: FS, networking, drivers, memory mgmt.\n");
    printf("  Pros: fast (no IPC overhead), shared data structures.\n");
    printf("  Cons: any bug can crash the entire system, large attack surface.\n");
    ps("Phase 2: Microkernel (Minix, seL4, QNX)");
    printf("  Minimal kernel: IPC, scheduling, memory. Everything else in userspace.\n");
    printf("  File server, network server, drivers all run as separate processes.\n");
    printf("  Pros: isolation (driver crash doesnt kill kernel), formally verifiable (seL4).\n");
    printf("  Cons: IPC overhead, complexity of message passing.\n");
    ps("Phase 3: Exokernel");
    printf("  Minimal kernel exposes raw hardware to applications.\n");
    printf("  Each application provides its own OS abstractions (libOS).\n");
    printf("  Pros: maximum flexibility and performance.\n");
    printf("  Modern descendant: Unikernels (run single app as kernel).\n");
    ps("Phase 4: Multikernel (Barrelfish)");
    printf("  Each core runs its own kernel instance. Cores communicate via messages.\n");
    printf("  Treats the machine as a distributed system.\n");
    printf("  Relevant to: NUMA servers, heterogeneous systems (CPU+GPU).\n");
    printf("  GPU DCs: heterogeneous scheduling across CPU/GPU/DPU is multikernel-like.\n");
    printf("\n========== Quiz ==========\n");
    printf("Q1. Why did Linux choose monolithic over microkernel?\n");
    printf("Q2. What is the Tanenbaum-Torvalds debate about?\n");
    printf("Q3. How does seL4 achieve formal verification?\n");
    printf("Q4. Why is the multikernel model relevant to GPU data centres?\n");
    return 0;}
