/*
 * lab_13.c
 * Topic: Setting up page tables for user processes
 * Build: gcc -O0 -Wall -pthread -o lab_13 lab_13_*.c
 *
 * Author: Arun Singh | arunsingh.in@gmail.com
 * OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/resource.h>
static void print_section(const char *t){printf("\n========== %s ==========\n",t);}
static void get_faults(long *mi,long *ma){struct rusage r;getrusage(RUSAGE_SELF,&r);*mi=r.ru_minflt;*ma=r.ru_majflt;}
int main(void){
    printf("=== Lab 13: Setting Up Page Tables for User Processes ===\n");
    print_section("Phase 1: Fork and COW Page Table Copy");
    long m0,j0,m1,j1;
    size_t sz=32*1024*1024;
    char *mem=mmap(NULL,sz,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    memset(mem,0xAA,sz);
    printf("  Parent: allocated and touched %zu MiB\n",sz/(1024*1024));
    get_faults(&m0,&j0);
    pid_t pid=fork();
    if(pid==0){
        get_faults(&m1,&j1);
        printf("  Child: faults after fork (before write): minor=%ld\n",m1-m0);
        get_faults(&m0,&j0);
        memset(mem,0xBB,sz);
        get_faults(&m1,&j1);
        printf("  Child: faults after writing %zu MiB: minor=%ld (COW copies)\n",sz/(1024*1024),m1-m0);
        _exit(0);
    }
    wait(NULL);
    printf("  Parent: mem[0]=0x%02x (unchanged by child COW)\n",(unsigned char)mem[0]);
    munmap(mem,sz);
    print_section("Phase 2: exec() Replaces Page Tables");
    printf("  When exec() is called, the kernel:\n");
    printf("  1. Releases the old mm_struct and all its VMAs\n");
    printf("  2. Creates a new mm_struct with new page tables\n");
    printf("  3. Maps the new executable: code, data, BSS, stack\n");
    printf("  4. Sets up the initial stack with argc, argv, envp\n");
    printf("  This is why fork()+exec() is cheap — fork COWs, exec replaces.\n");
    printf("\n========== Quiz ==========\n");
    printf("Q1. How many page faults does fork() itself cause? Why is it fast?\n");
    printf("Q2. When does the child actually get its own physical pages?\n");
    printf("Q3. What happens to the parent page tables when exec() is called in the child?\n");
    printf("Q4. Why is vfork() sometimes faster than fork() for exec()-only children?\n");
    return 0;
}
