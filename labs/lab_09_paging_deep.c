/*
 * lab_09_paging_deep.c
 * Topic: Paging (deep dive)
 * Explores page table walk, PTE flags, page fault types.
 * Build: gcc -O0 -Wall -o lab_09 lab_09_paging_deep.c
 *
 * Author: Arun Singh | arunsingh.in@gmail.com
 * OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/resource.h>

static void print_section(const char *t) { printf("\n========== %s ==========\n", t); }

static void get_faults(long *min, long *maj) {
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);
    *min = ru.ru_minflt; *maj = ru.ru_majflt;
}

static void demo_pte_flags(void) {
    print_section("Phase 1: Page Table Entry Flags");
    printf("  Each PTE contains:\n");
    printf("    bit 0  (P)   = Present\n");
    printf("    bit 1  (R/W) = Read/Write (0=read-only, 1=writable)\n");
    printf("    bit 2  (U/S) = User/Supervisor (0=kernel-only, 1=user accessible)\n");
    printf("    bit 5  (A)   = Accessed (set by CPU on access)\n");
    printf("    bit 6  (D)   = Dirty (set by CPU on write)\n");
    printf("    bit 7  (PS)  = Page Size (1=large page: 2MiB at PD level, 1GiB at PDP)\n");
    printf("    bit 63 (NX)  = No-Execute (if EFER.NXE=1)\n\n");
    printf("  These flags are how the kernel implements:\n");
    printf("    - COW: mark pages R/O, trap on write, copy then remap R/W\n");
    printf("    - Demand paging: mark not-present, trap on access, allocate then map\n");
    printf("    - NX protection: mark data pages no-execute to prevent code injection\n");
}

static void demo_fault_counting(void) {
    print_section("Phase 2: Page Fault Types — Live Counting");
    long min0, maj0, min1, maj1;
    size_t sz = 16 * 1024 * 1024; /* 16 MiB */

    get_faults(&min0, &maj0);
    void *p = mmap(NULL, sz, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    get_faults(&min1, &maj1);
    printf("  After mmap(16MiB): minor_faults=%ld (mapping metadata only)\n", min1-min0);

    get_faults(&min0, &maj0);
    memset(p, 0xAB, sz);
    get_faults(&min1, &maj1);
    printf("  After memset(16MiB): minor_faults=%ld (~%ld pages touched)\n",
           min1-min0, sz / (size_t)sysconf(_SC_PAGESIZE));

    get_faults(&min0, &maj0);
    memset(p, 0xCD, sz);
    get_faults(&min1, &maj1);
    printf("  After 2nd memset:    minor_faults=%ld (pages already present)\n", min1-min0);

    munmap(p, sz);
    printf("\n  OBSERVE: First touch = many faults. Re-touch = few/zero faults.\n");
}

static void demo_smaps(void) {
    print_section("Phase 3: Detailed VMA Info (/proc/self/smaps)");
    size_t sz = 4 * 1024 * 1024;
    void *p = mmap(NULL, sz, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    memset(p, 0xEF, sz);

    FILE *fp = fopen("/proc/self/smaps", "r");
    if (!fp) { munmap(p, sz); return; }
    char line[512];
    int found = 0;
    char target[32];
    snprintf(target, sizeof(target), "%lx", (unsigned long)p);
    while (fgets(line, sizeof(line), fp)) {
        if (!found && strstr(line, target)) found = 1;
        if (found) {
            printf("  %s", line);
            if (strncmp(line, "VmFlags", 7) == 0) break;
        }
    }
    fclose(fp);
    munmap(p, sz);
    printf("\n  smaps shows per-VMA: Size, Rss, Pss, Referenced, Anonymous, etc.\n");
}

int main(void) {
    printf("=== Lab 09: Paging (Deep Dive) ===\n");
    demo_pte_flags();
    demo_fault_counting();
    demo_smaps();
    printf("\n========== Quiz ==========\n");
    printf("Q1. What PTE flag change implements Copy-on-Write?\n");
    printf("Q2. Why does the second memset cause near-zero faults?\n");
    printf("Q3. What is the 'Dirty' bit used for by the kernel?\n");
    printf("Q4. What is PSS (Proportional Set Size) and why is it more useful than RSS for shared libs?\n");
    printf("Q5. How does NX bit prevent buffer overflow exploits?\n");
    return 0;
}
