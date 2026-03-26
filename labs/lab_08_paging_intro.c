/*
 * lab_08_paging_intro.c
 * Topic: Segmentation Review, Introduction to Paging
 * Explores page size, /proc/self/pagemap, virtual-to-physical translation.
 * Build: gcc -O0 -Wall -o lab_08 lab_08_paging_intro.c
 * Run:   sudo ./lab_08   (pagemap requires CAP_SYS_ADMIN)
 *
 * Author: Arun Singh | arunsingh.in@gmail.com
 * OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

static void print_section(const char *t) { printf("\n========== %s ==========\n", t); }

static uint64_t virt_to_phys(void *addr) {
    int fd = open("/proc/self/pagemap", O_RDONLY);
    if (fd < 0) return 0;
    uint64_t page_size = (uint64_t)sysconf(_SC_PAGESIZE);
    uint64_t vpn = (uint64_t)addr / page_size;
    uint64_t entry;
    if (pread(fd, &entry, sizeof(entry), (off_t)(vpn * 8)) != sizeof(entry)) {
        close(fd); return 0;
    }
    close(fd);
    if (!(entry & (1ULL << 63))) return 0; /* not present */
    uint64_t pfn = entry & ((1ULL << 55) - 1);
    return pfn * page_size + ((uint64_t)addr % page_size);
}

static void demo_page_size(void) {
    print_section("Phase 1: Page Size");
    long ps = sysconf(_SC_PAGESIZE);
    printf("  System page size: %ld bytes (%ld KiB)\n", ps, ps / 1024);
    printf("  x86_64 supports: 4KiB (standard), 2MiB (huge), 1GiB (giant)\n");
    printf("  Page size determines: TLB coverage, internal fragmentation, mmap granularity\n");
}

static void demo_pagemap(void) {
    print_section("Phase 2: Virtual-to-Physical Translation (/proc/self/pagemap)");
    printf("  Each virtual page has a pagemap entry with:\n");
    printf("    bit 63 = present, bits 0-54 = PFN (Page Frame Number)\n\n");

    int var_on_stack = 42;
    int *var_on_heap = malloc(sizeof(int));
    *var_on_heap = 99;
    void *var_mmap = mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    *(int*)var_mmap = 77;

    struct { const char *name; void *addr; } regions[] = {
        {"stack",  &var_on_stack},
        {"heap",   var_on_heap},
        {"mmap",   var_mmap},
        {"code",   (void *)demo_pagemap},
        {NULL, NULL}
    };

    for (int i = 0; regions[i].name; i++) {
        uint64_t pa = virt_to_phys(regions[i].addr);
        printf("  %-6s  VA=%p  PA=0x%lx  %s\n",
               regions[i].name, regions[i].addr, pa,
               pa ? "" : "(need sudo or CAP_SYS_ADMIN)");
    }

    printf("\n  OBSERVE: Different virtual addresses map to different physical frames.\n");
    printf("  The page table (multi-level on x86_64: PML4->PDP->PD->PT) does this translation.\n");
    free(var_on_heap);
    munmap(var_mmap, 4096);
}

static void demo_multilevel(void) {
    print_section("Phase 3: Multi-Level Page Table (x86_64)");
    printf("  x86_64 uses 4-level (or 5-level with LA57) page tables:\n\n");
    printf("  Virtual Address (48-bit):\n");
    printf("  ┌────────┬────────┬────────┬────────┬────────────┐\n");
    printf("  │PML4[9] │PDP[9]  │PD[9]   │PT[9]   │Offset[12]  │\n");
    printf("  └────────┴────────┴────────┴────────┴────────────┘\n");
    printf("   bits 47:39  38:30  29:21  20:12     11:0\n\n");
    printf("  Each level has 512 entries (9 bits each).\n");
    printf("  512 * 512 * 512 * 512 * 4KiB = 256 TiB addressable (48-bit VA).\n");
    printf("  With 5-level (LA57): 57-bit VA = 128 PiB.\n");
}

int main(void) {
    printf("=== Lab 08: Segmentation Review, Introduction to Paging ===\n");
    demo_page_size();
    demo_pagemap();
    demo_multilevel();
    printf("\n========== Quiz ==========\n");
    printf("Q1. How many levels does the x86_64 page table have? How many bits does each level index?\n");
    printf("Q2. What is a PFN (Page Frame Number)?\n");
    printf("Q3. Why can't a normal user read /proc/self/pagemap? What security risk would it pose?\n");
    printf("Q4. What happens when the CPU accesses a virtual address whose page table entry is 'not present'?\n");
    printf("Q5. Why does x86_64 support 5-level page tables (LA57)?\n");
    return 0;
}
