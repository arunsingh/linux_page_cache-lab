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

/* WHY: 5-level paging (LA57, enabled via CR4.LA57) extends x86_64 to 57-bit virtual
 *      addresses = 128 PiB per process.  This is needed for servers with >128 TiB RAM
 *      (the 4-level limit) -- AMD EPYC 9004 series supports up to 6 TB of DRAM per socket.
 *      Linux enables LA57 at boot if the CPU and BIOS support it.  Check:
 *        grep la57 /proc/cpuinfo
 *      A 5-level page table adds one extra PML5 level (9 bits), so each walk is 5 memory
 *      reads vs 4.  This slightly increases TLB miss penalty but enables much larger memory.
 *
 * WHY: mmap(MAP_HUGETLB) allocates 2 MiB huge pages directly.  This reduces page table
 *      size (1 PDE per 2MiB vs 512 PTEs) and TLB pressure (1 TLB entry per 2MiB vs 512).
 *      For ML workloads loading a 70B parameter model (140+ GiB), huge pages can reduce
 *      page fault time and TLB miss rate significantly.
 */

static void demo_page_alignment(void) {
    print_section("Phase 4: Page-Aligned Allocations and Huge Pages");

    long ps = sysconf(_SC_PAGESIZE);
    printf("  Standard page size: %ld bytes\n", ps);

    /* Allocate aligned pages */
    void *p1 = mmap(NULL, (size_t)ps, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    void *p2 = mmap(NULL, (size_t)ps, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if (p1 == MAP_FAILED || p2 == MAP_FAILED) { perror("mmap"); return; }

    /* OBSERVE: mmap always returns page-aligned addresses */
    printf("  p1 = %p  (page-aligned: %s)\n", p1, ((uintptr_t)p1 % (uintptr_t)ps == 0) ? "YES" : "NO");
    printf("  p2 = %p  (page-aligned: %s)\n", p2, ((uintptr_t)p2 % (uintptr_t)ps == 0) ? "YES" : "NO");
    printf("  Distance p2-p1: %ld bytes (~%ld pages)\n",
           (long)((char *)p2 - (char *)p1), (long)((char *)p2 - (char *)p1) / ps);

    /* Touch the pages to make them resident */
    *(volatile char *)p1 = 0xAA;
    *(volatile char *)p2 = 0xBB;

    /* Try huge pages */
    printf("\n  Attempting 2 MiB huge page allocation (MAP_HUGETLB)...\n");
    void *huge = mmap(NULL, 2 * 1024 * 1024,
                      PROT_READ|PROT_WRITE,
                      MAP_PRIVATE|MAP_ANONYMOUS|MAP_HUGETLB, -1, 0);
    if (huge == MAP_FAILED) {
        printf("  MAP_HUGETLB failed (no huge pages reserved).\n");
        printf("  To enable: echo 32 > /proc/sys/vm/nr_hugepages\n");
        printf("  WHY: Huge pages are pre-allocated from boottime pool; not on-demand.\n");
    } else {
        printf("  Huge page allocated at %p\n", huge);
        printf("  Address mod 2MiB = %lu (should be 0: %s)\n",
               (unsigned long)((uintptr_t)huge % (2*1024*1024)),
               ((uintptr_t)huge % (2*1024*1024) == 0) ? "YES" : "NO");
        munmap(huge, 2 * 1024 * 1024);
    }

    munmap(p1, (size_t)ps);
    munmap(p2, (size_t)ps);

    /* WHY: 5-level paging check */
    printf("\n  Checking 5-level paging support:\n");
    FILE *fp = fopen("/proc/cpuinfo", "r");
    if (fp) {
        char line[512]; int found = 0;
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "flags", 5) == 0 && strstr(line, "la57")) {
                printf("  CPU supports LA57 (5-level paging): YES\n");
                found = 1; break;
            }
        }
        if (!found) printf("  LA57 (5-level paging): not in /proc/cpuinfo flags\n");
        fclose(fp);
    }
    printf("  5-level paging needed for >128 TiB VA space (servers with >128 TiB RAM).\n");
}

int main(void) {
    /* EXPECTED OUTPUT (Linux x86_64):
     *   Phase 1: System page size: 4096 bytes (4 KiB)
     *   Phase 2: stack/heap/mmap/code VAs with physical addresses (need sudo for PAs)
     *   Phase 3: 4-level page table diagram, 48-bit VA space = 256 TiB
     *   Phase 4: p1 and p2 page-aligned; huge page attempt (success or failure message);
     *            LA57 flag check
     */
    printf("=== Lab 08: Segmentation Review, Introduction to Paging ===\n");
    demo_page_size();
    demo_pagemap();
    demo_multilevel();
    demo_page_alignment();

    /* === EXERCISE ===
     * Try these hands-on tasks:
     * 1. Run this as root (sudo ./lab_08) to get actual physical addresses from
     *    /proc/self/pagemap.  Verify that two adjacent virtual pages do NOT
     *    necessarily map to adjacent physical frames (physical fragmentation).
     *    Then fork() and compare the child's physical addresses to the parent's --
     *    before any write they should be identical (shared COW pages).
     * 2. Enable huge pages and allocate with MAP_HUGETLB:
     *      sudo sh -c 'echo 64 > /proc/sys/vm/nr_hugepages'
     *    Re-run the program and confirm huge page allocation succeeds.
     *    Compare page fault count (getrusage) for touching 64 MiB with 4K pages
     *    vs 2 MiB pages -- the huge page version should have ~512x fewer faults.
     * 3. Modern (5-level paging for >128 TiB servers): check if your CPU supports
     *    LA57: grep la57 /proc/cpuinfo.  If running on an AMD EPYC 9004 or Intel
     *    Sapphire Rapids with >128 TiB RAM, the OS will have LA57 active.  The extra
     *    page table level means one additional memory access per TLB miss -- measure
     *    the impact with a TLB-thrashing benchmark (random access to large array).
     *
     * OBSERVE: mmap() always returns addresses aligned to the page size (4096 bytes).
     *          MAP_HUGETLB returns addresses aligned to the huge page size (2 MiB).
     *          The alignment is enforced by the kernel's mmap_region() function.
     * WHY:     Page alignment is required because page tables map entire pages, not
     *          bytes.  An unaligned mmap would map more memory than requested,
     *          potentially revealing adjacent allocations to the user.
     */
    printf("\n========== Hands-On Exercise ==========\n");
    printf("1. Run as root; use /proc/self/pagemap to get physical addresses;\n");
    printf("   verify virtual-adjacent pages are not physically adjacent.\n");
    printf("2. Enable 64 huge pages; compare page fault count for 64 MiB touch\n");
    printf("   with 4K pages vs 2 MiB huge pages (expect ~512x fewer faults).\n");
    printf("3. Modern (5-level paging): check la57 flag in /proc/cpuinfo;\n");
    printf("   explain why >128 TiB servers need it and the TLB miss cost trade-off.\n");

    printf("\n========== Quiz ==========\n");
    printf("Q1. How many levels does the x86_64 page table have? How many bits does each level index?\n");
    printf("Q2. What is a PFN (Page Frame Number)?\n");
    printf("Q3. Why can't a normal user read /proc/self/pagemap? What security risk would it pose?\n");
    printf("Q4. What happens when the CPU accesses a virtual address whose page table entry is 'not present'?\n");
    printf("Q5. Why does x86_64 support 5-level page tables (LA57)?\n");
    printf("Q6. What is the difference between MAP_HUGETLB and Transparent Huge Pages (THP)?\n");
    printf("    Which requires pre-allocation and which is automatic?\n");
    printf("Q7. For an ML workload loading a 140 GiB model, how does using 2 MiB huge\n");
    printf("    pages reduce page fault count and TLB miss rate compared to 4 KiB pages?\n");
    return 0;
}
