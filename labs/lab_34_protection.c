/*
 * lab_34_protection.c
 * Topic: Protection and Security
 * Build: gcc -O0 -Wall -pthread -o lab_34 lab_34_protection.c
 *
 * Author: Arun Singh | arunsingh.in@gmail.com
 * OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
*/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/prctl.h>
/* sys/capability.h — install libcap-dev if available */
#include <linux/seccomp.h>
#include <sys/syscall.h>
#include <sched.h>
static void ps(const char *t){printf("\n========== %s ==========\n",t);}
int main(void){
    printf("=== Lab 34: Protection and Security ===\n");
    ps("Phase 1: Linux Capabilities");
    printf("  Traditional: root (UID 0) can do everything.\n");
    printf("  Capabilities: fine-grained privileges (CAP_NET_ADMIN, CAP_SYS_PTRACE, etc.)\n\n");
    printf("  This process (PID %d) capabilities:\n",getpid());
    char path[64];snprintf(path,sizeof(path),"/proc/%d/status",getpid());
    FILE *fp=fopen(path,"r");
    if(fp){char l[256];while(fgets(l,sizeof(l),fp))if(strncmp(l,"Cap",3)==0)printf("  %s",l);fclose(fp);}
    ps("Phase 2: Namespaces");
    printf("  Linux namespaces isolate system resources per process group:\n");
    printf("    PID ns:  separate PID numbering (containers see PID 1)\n");
    printf("    NET ns:  separate network stack\n");
    printf("    MNT ns:  separate mount table\n");
    printf("    UTS ns:  separate hostname\n");
    printf("    USER ns: separate UID/GID mapping\n");
    printf("    IPC ns:  separate IPC resources\n");
    printf("  Docker/containers use ALL of these together.\n");
    ps("Phase 3: Seccomp");
    printf("  Seccomp filters restrict which syscalls a process can make.\n");
    printf("  Used by: Chrome, Docker, Android, systemd.\n");
    printf("  Mode 1: only read/write/exit/sigreturn.\n");
    printf("  Mode 2 (BPF): custom filter program per syscall.\n");
    ps("Phase 4: ASLR");
    printf("  Address Space Layout Randomization:\n");
    void *stack_var;
    printf("    Stack address:  %p (changes each run)\n",(void*)&stack_var);
    printf("    Heap address:   %p\n",malloc(1));
    printf("    Code address:   %p\n",(void*)main);
    printf("  ASLR makes exploits harder by randomizing memory layout.\n");
    printf("\n========== Quiz ==========\n");
    printf("Q1. Why are capabilities better than all-or-nothing root?\n");
    printf("Q2. How do PID namespaces make containers think they have PID 1?\n");
    printf("Q3. What is seccomp-bpf and how does Docker use it?\n");
    printf("Q4. Can ASLR be bypassed? How?\n");
    return 0;}
