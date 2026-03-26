/*
 * lab_07_kernel_ds_mm.c
 * Topic: Kernel Data Structures, Memory Management
 * Explores /proc/slabinfo, /proc/buddyinfo, mm_struct via /proc/self/maps.
 * Build: gcc -O0 -Wall -o lab_07 lab_07_kernel_ds_mm.c
 *
 * Author: Arun Singh | arunsingh.in@gmail.com
 * OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

static void print_section(const char *t) { printf("\n========== %s ==========\n", t); }

static void demo_slab_allocator(void) {
    print_section("Phase 1: Slab Allocator (/proc/slabinfo)");
    printf("  The kernel uses slab/slub allocator for fixed-size objects:\n");
    printf("  task_struct, inode, dentry, mm_struct, etc.\n\n");
    FILE *fp = fopen("/proc/slabinfo", "r");
    if (!fp) { printf("  (need root: sudo cat /proc/slabinfo)\n"); return; }
    char line[512]; int n = 0;
    while (fgets(line, sizeof(line), fp) && n++ < 3) printf("  %s", line);
    printf("  ...\n");
    /* Find task_struct */
    rewind(fp); n = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "task_struct") || strstr(line, "mm_struct") ||
            strstr(line, "inode_cache") || strstr(line, "dentry")) {
            printf("  %s", line);
            if (++n >= 6) break;
        }
    }
    fclose(fp);
    printf("\n  OBSERVE: Object caches pre-allocate frequently used kernel structures.\n");
}

static void demo_buddy_allocator(void) {
    print_section("Phase 2: Buddy Allocator (/proc/buddyinfo)");
    printf("  Physical page allocation uses the buddy system: power-of-2 page blocks.\n\n");
    FILE *fp = fopen("/proc/buddyinfo", "r");
    if (!fp) { perror("fopen"); return; }
    char line[256];
    while (fgets(line, sizeof(line), fp)) printf("  %s", line);
    fclose(fp);
    printf("\n  Each column = count of free blocks of order 0,1,...,10 (1,2,...,1024 pages)\n");
    printf("  Fragmentation = plenty of small blocks but no large ones.\n");
}

static void demo_vmstat(void) {
    print_section("Phase 3: VM Statistics (/proc/vmstat highlights)");
    FILE *fp = fopen("/proc/vmstat", "r");
    if (!fp) { perror("fopen"); return; }
    char line[256];
    const char *keys[] = {"nr_free_pages", "nr_active_anon", "nr_inactive_anon",
                          "nr_active_file", "nr_inactive_file", "pgfault",
                          "pgmajfault", "pswpin", "pswpout", NULL};
    while (fgets(line, sizeof(line), fp)) {
        for (int i = 0; keys[i]; i++) {
            if (strncmp(line, keys[i], strlen(keys[i])) == 0) { printf("  %s", line); break; }
        }
    }
    fclose(fp);
}

static void demo_mm_struct(void) {
    print_section("Phase 4: mm_struct — Process Address Space");
    printf("  Each process has an mm_struct containing its VMA (Virtual Memory Area) list.\n");
    printf("  /proc/self/maps shows the VMAs. /proc/self/status shows summary stats.\n\n");
    FILE *fp = fopen("/proc/self/status", "r");
    if (!fp) return;
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "Vm", 2) == 0 || strncmp(line, "Rss", 3) == 0 ||
            strncmp(line, "Threads", 7) == 0)
            printf("  %s", line);
    }
    fclose(fp);
    printf("\n  VmSize  = total virtual address space\n");
    printf("  VmRSS   = resident (physically backed) pages\n");
    printf("  VmData  = private data mappings\n");
    printf("  VmStk   = stack size\n");
}

int main(void) {
    printf("=== Lab 07: Kernel Data Structures, Memory Management ===\n");
    demo_slab_allocator();
    demo_buddy_allocator();
    demo_vmstat();
    demo_mm_struct();
    printf("\n========== Quiz ==========\n");
    printf("Q1. Why does the kernel use slab allocation instead of malloc?\n");
    printf("Q2. Explain the buddy allocator and how it handles fragmentation.\n");
    printf("Q3. What is an mm_struct and which processes share one?\n");
    printf("Q4. What do pgfault and pgmajfault counts in /proc/vmstat represent?\n");
    printf("Q5. How would you detect memory fragmentation on a production server?\n");
    return 0;
}
