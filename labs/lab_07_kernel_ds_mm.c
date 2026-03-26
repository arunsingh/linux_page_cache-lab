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

/* WHY: SLUB (Simple List of Unused Blocks) is the default slab allocator since
 *      Linux 2.6.23, replacing the original SLAB.  Key differences:
 *      - SLUB uses per-CPU slabs (no per-CPU caches like SLAB), reducing cache footprint.
 *      - Objects are allocated from partial/full slabs using a single freelist pointer.
 *      - SLUB has better memory utilization on NUMA systems (NUMA-aware allocation).
 *      - SLUB debug mode (/sys/kernel/slab/<cache>/alloc_fastpath, etc.) provides
 *        detailed allocation statistics.
 *      In Linux 6.x, SLUB is the only maintained allocator (SLAB and SLOB removed in 6.2).
 */

static void demo_slub_stats(void) {
    print_section("Phase 5: SLUB Allocator Statistics (Linux 6.x)");
    printf("  SLUB is the default (and since 6.2: only) slab allocator in Linux.\n\n");

    /* OBSERVE: /sys/kernel/slab/ has one directory per cache with detailed stats */
    FILE *fp;
    char path[256], val[64];

    /* Check a well-known slab cache */
    const char *caches[] = {"kmalloc-64", "kmalloc-128", "kmalloc-256", NULL};
    for (int i = 0; caches[i]; i++) {
        snprintf(path, sizeof(path), "/sys/kernel/slab/%s/object_size", caches[i]);
        fp = fopen(path, "r");
        if (!fp) continue;
        if (fgets(val, sizeof(val), fp)) {
            val[strcspn(val, "\n")] = '\0';
            printf("  %-20s object_size=%s bytes\n", caches[i], val);
        }
        fclose(fp);

        snprintf(path, sizeof(path), "/sys/kernel/slab/%s/slabs_cpu_partial", caches[i]);
        fp = fopen(path, "r");
        if (fp) {
            if (fgets(val, sizeof(val), fp)) {
                val[strcspn(val, "\n")] = '\0';
                printf("  %-20s slabs_cpu_partial=%s\n", caches[i], val);
            }
            fclose(fp);
        }
    }

    /* WHY: kmalloc-64 is used for small kernel allocations (e.g., file descriptors,
     *      small network packet headers).  The kernel rounds up allocation sizes to
     *      the next power-of-2 slab cache.  kmalloc(50) uses kmalloc-64. */
    printf("\n  WHY: kernel kmalloc() rounds up to nearest cache (like a power-of-2 allocator).\n");
    printf("  kmalloc(50) -> uses kmalloc-64 slab (wastes 14 bytes per object).\n");
    printf("  Dedicated caches (task_struct, inode) avoid this waste for hot objects.\n");
}

int main(void) {
    /* EXPECTED OUTPUT (Linux x86_64):
     *   Phase 1: slabinfo shows task_struct, mm_struct, inode_cache, dentry entries
     *            (requires root; otherwise prints "need root" message)
     *   Phase 2: buddyinfo shows free blocks per order per NUMA node+zone
     *   Phase 3: vmstat shows pgfault, pgmajfault, pswpin, pswpout counters
     *   Phase 4: /proc/self/status Vm* fields (VmSize, VmRSS, VmData, VmStk, Threads)
     *   Phase 5: SLUB object sizes from /sys/kernel/slab/ (Linux 6.x)
     */
    printf("=== Lab 07: Kernel Data Structures, Memory Management ===\n");
    demo_slab_allocator();
    demo_buddy_allocator();
    demo_vmstat();
    demo_mm_struct();
    demo_slub_stats();

    /* === EXERCISE ===
     * Try these hands-on tasks:
     * 1. Read /proc/slabinfo (as root) and find the task_struct cache.
     *    Record <active_objs> and <objsize>.  Compare objsize to
     *    'grep task_struct /proc/slabinfo' vs 'pahole -C task_struct vmlinux'
     *    (pahole requires kernel debug symbols).  Does the size match kernel source?
     * 2. Measure buddy allocator fragmentation: parse /proc/buddyinfo and compute
     *    the largest contiguous free block (highest order with count > 0) in each zone.
     *    Then allocate a large buffer with mmap(MAP_HUGETLB) and re-check buddyinfo.
     * 3. Modern (SLUB allocator in Linux 6.x): read /sys/kernel/slab/kmalloc-256/
     *    directory.  List all files and read alloc_fastpath and alloc_slowpath.
     *    These count how often SLUB serves from the per-CPU freelist (fast) vs
     *    the global partial list (slow).  High slowpath ratio = CPU cache pressure.
     *
     * OBSERVE: The mm_struct is reference-counted (mm_count, mm_users fields in kernel).
     *          Threads in the same process share mm_struct (same mm_users count).
     *          Fork creates a new mm_struct (copy_mm) with COW page tables.
     * WHY:     SLUB removed per-CPU caches from SLAB design, instead using a single
     *          per-CPU 'active slab' pointer.  This reduces cache footprint from O(N_CPU
     *          * N_caches) to O(N_caches), critical on 256+ core machines.
     */
    printf("\n========== Hands-On Exercise ==========\n");
    printf("1. As root, read /proc/slabinfo; find task_struct size and active object count.\n");
    printf("2. Parse /proc/buddyinfo; find largest contiguous free block per zone.\n");
    printf("3. Modern (SLUB Linux 6.x): read /sys/kernel/slab/kmalloc-256/alloc_fastpath\n");
    printf("   and alloc_slowpath to measure per-CPU vs global freelist utilization.\n");

    printf("\n========== Quiz ==========\n");
    printf("Q1. Why does the kernel use slab allocation instead of malloc?\n");
    printf("Q2. Explain the buddy allocator and how it handles fragmentation.\n");
    printf("Q3. What is an mm_struct and which processes share one?\n");
    printf("Q4. What do pgfault and pgmajfault counts in /proc/vmstat represent?\n");
    printf("Q5. How would you detect memory fragmentation on a production server?\n");
    printf("Q6. How does SLUB differ from the original SLAB allocator, and why was SLAB\n");
    printf("    removed from the Linux kernel in version 6.2?\n");
    printf("Q7. What is the mm_count vs mm_users distinction in mm_struct, and which\n");
    printf("    counter is decremented when a thread exits vs when a process exits?\n");
    return 0;
}
