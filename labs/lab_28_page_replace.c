/*
 * lab_28_page_replace.c
 * Topic: Page Replacement, Thrashing
 * Build: gcc -O0 -Wall -pthread -o lab_28 lab_28_page_replace.c
 *
 * Author: Arun Singh | arunsingh.in@gmail.com
 * OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
*/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <time.h>
static void ps(const char *t){printf("\n========== %s ==========\n",t);}
static double now(void){struct timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);return ts.tv_sec+ts.tv_nsec/1e9;}
int main(void){
    printf("=== Lab 28: Page Replacement, Thrashing ===\n");
    ps("Phase 1: Working Set Concept");
    printf("  Working set = set of pages a process actively uses.\n");
    printf("  If working set fits in RAM -> fast. If not -> thrashing.\n\n");
    /* Sequential vs random access pattern */
    size_t sz=64*1024*1024;
    char *mem=mmap(NULL,sz,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    memset(mem,1,sz);
    /* Sequential = good locality */
    double t0=now();long s=0;
    for(int r=0;r<3;r++)for(size_t i=0;i<sz;i+=4096)s+=mem[i];
    printf("  Sequential scan (64MiB, 3x): %.3fs\n",now()-t0);
    /* Random = poor locality */
    srand(42);t0=now();
    for(int i=0;i<(int)(sz/4096)*3;i++)s+=mem[(rand()%(sz/4096))*4096];
    printf("  Random access (same count):   %.3fs\n",now()-t0);
    printf("  OBSERVE: Random access is slower due to poor spatial/temporal locality.\n");
    munmap(mem,sz);
    ps("Phase 2: Linux Page Replacement");
    printf("  Linux uses a modified LRU with active/inactive lists:\n");
    printf("  - Active list:   recently accessed pages (hot)\n");
    printf("  - Inactive list: candidates for eviction (cold)\n");
    printf("  - Pages promote active->inactive->eviction\n");
    printf("  - Accessed bit from PTE drives promotion/demotion\n\n");
    FILE *fp=fopen("/proc/meminfo","r");
    if(fp){char l[256];
        while(fgets(l,sizeof(l),fp)){
            if(strstr(l,"Active")||strstr(l,"Inactive")||strstr(l,"Unevictable"))
                printf("  %s",l);}
        fclose(fp);}
    ps("Phase 3: Thrashing Detection");
    printf("  Thrashing = system spends more time paging than doing useful work.\n");
    printf("  Symptoms: high pgmajfault in /proc/vmstat, load average spikes.\n");
    printf("  Solutions: add RAM, reduce working set, use cgroup memory limits.\n");
    printf("  GPU DCs: thrashing in host memory delays GPU data pipeline.\n");
    printf("\n========== Quiz ==========\n");
    printf("Q1. Explain the working set model and why it matters for performance.\n");
    printf("Q2. How does Linux approximate LRU without scanning all pages?\n");
    printf("Q3. What is the difference between page-out and swap-out?\n");
    printf("Q4. How would you detect thrashing on a production server?\n");
    return 0;}
