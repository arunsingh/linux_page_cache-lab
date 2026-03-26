/*
 * lab_10_addr_space_paging.c
 * Topic: Process Address Spaces Using Paging
 * Demonstrates VMA layout, mmap, mprotect, address space after fork.
 * Build: gcc -O0 -Wall -o lab_10 lab_10_addr_space_paging.c
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

static void print_section(const char *t) { printf("\n========== %s ==========\n", t); }

static int count_vmas(void) {
    FILE *fp = fopen("/proc/self/maps", "r");
    if (!fp) return -1;
    int n = 0; char line[512];
    while (fgets(line, sizeof(line), fp)) n++;
    fclose(fp);
    return n;
}

static void demo_vma_growth(void) {
    print_section("Phase 1: VMA Growth with mmap");
    int base = count_vmas();
    printf("  Base VMAs: %d\n", base);

    void *pages[10];
    for (int i = 0; i < 10; i++) {
        pages[i] = mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
        printf("  After mmap #%d: VMAs=%d  addr=%p\n", i+1, count_vmas(), pages[i]);
    }
    for (int i = 0; i < 10; i++) munmap(pages[i], 4096);
    printf("  After munmap all: VMAs=%d\n", count_vmas());
    printf("\n  OBSERVE: Each mmap creates a new VMA in the mm_struct's VMA tree.\n");
}

static void demo_mprotect_split(void) {
    print_section("Phase 2: mprotect Splits VMAs");
    size_t ps = (size_t)sysconf(_SC_PAGESIZE);
    void *region = mmap(NULL, ps * 4, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    int before = count_vmas();
    /* Change middle 2 pages to read-only — splits 1 VMA into 3 */
    mprotect((char *)region + ps, ps * 2, PROT_READ);
    int after = count_vmas();
    printf("  VMAs before mprotect: %d, after: %d (expected: +2 from split)\n", before, after);
    munmap(region, ps * 4);
    printf("  OBSERVE: Changing permissions on a sub-range splits the VMA.\n");
}

static void demo_cow_after_fork(void) {
    print_section("Phase 3: COW Address Space After Fork");
    size_t ps = (size_t)sysconf(_SC_PAGESIZE);
    volatile int *shared_page = mmap(NULL, ps, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    *shared_page = 100;

    printf("  Parent: value=%d at %p\n", *shared_page, (void *)shared_page);
    pid_t pid = fork();
    if (pid == 0) {
        printf("  Child before write: value=%d at %p (same VA, COW page)\n", *shared_page, (void *)shared_page);
        *shared_page = 999;
        printf("  Child after write:  value=%d (COW triggered, private copy)\n", *shared_page);
        _exit(0);
    }
    wait(NULL);
    printf("  Parent after child exit: value=%d (unchanged — COW isolation)\n", *shared_page);
    munmap((void *)shared_page, ps);
}

int main(void) {
    printf("=== Lab 10: Process Address Spaces Using Paging ===\n");
    demo_vma_growth();
    demo_mprotect_split();
    demo_cow_after_fork();
    printf("\n========== Quiz ==========\n");
    printf("Q1. What kernel data structure tracks VMAs? (hint: maple tree in 6.x, rb-tree before)\n");
    printf("Q2. Why does mprotect on a sub-range split a VMA?\n");
    printf("Q3. Explain the COW sequence: fork -> read -> write -> page fault -> copy.\n");
    printf("Q4. What happens to the parent's page tables when the child does exec()?\n");
    printf("Q5. Why does each shared library (.so) create multiple VMAs (r-x, r--, rw-)?\n");
    return 0;
}
