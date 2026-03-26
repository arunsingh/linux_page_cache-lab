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

/* WHY: VMA merging: when two adjacent VMAs have identical flags, permissions, and
 *      file backing, the kernel merges them into one VMA.  This reduces VMA count
 *      and speeds up page table operations.  munmap() also triggers merging.
 *      MAP_FIXED can replace an existing mapping precisely; it will merge with adjacent
 *      VMAs if conditions match.  MAP_FIXED_NOREPLACE (Linux 4.17) adds safety: fails
 *      instead of replacing existing mappings.
 *
 * WHY: Huge pages in ML workloads:  PyTorch and TensorFlow allocate large contiguous
 *      tensors.  With Transparent Huge Pages (THP, controlled via
 *      /sys/kernel/mm/transparent_hugepage/enabled), the kernel automatically promotes
 *      aligned 2 MiB anonymous regions to huge pages.  This reduces TLB pressure for
 *      large model weight tensors (GPT-4 has ~1 TiB of weights during training).
 *      NVIDIA's CUDA runtime also uses THP for GPU-visible allocations on the host.
 */

static void demo_maps_before_after_mmap(void) {
    print_section("Phase 4: /proc/self/maps Before vs After mmap");

    int vmas_before = count_vmas();
    printf("  VMAs before new mmap: %d\n", vmas_before);

    /* Allocate a 4 MiB region */
    size_t sz = 4 * 1024 * 1024;
    void *p = mmap(NULL, sz, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) { perror("mmap"); return; }

    int vmas_after_alloc = count_vmas();
    printf("  VMAs after mmap(4MiB): %d (+%d)\n", vmas_after_alloc, vmas_after_alloc - vmas_before);

    /* OBSERVE: The new VMA appears in /proc/self/maps */
    FILE *fp = fopen("/proc/self/maps", "r");
    char line[512]; char target[32];
    snprintf(target, sizeof(target), "%lx", (unsigned long)p);
    if (fp) {
        while (fgets(line, sizeof(line), fp)) {
            if (strstr(line, target)) { printf("  New VMA: %s", line); break; }
        }
        fclose(fp);
    }

    /* Split the VMA with different permissions in the middle */
    size_t ps = (size_t)sysconf(_SC_PAGESIZE);
    mprotect((char *)p + ps, ps, PROT_READ); /* Middle page: read-only */
    int vmas_after_split = count_vmas();
    printf("  VMAs after mprotect(middle page RO): %d (+%d from split)\n",
           vmas_after_split, vmas_after_split - vmas_after_alloc);

    /* Restore and observe merge */
    mprotect((char *)p + ps, ps, PROT_READ|PROT_WRITE);
    int vmas_after_merge = count_vmas();
    printf("  VMAs after mprotect(restore RW): %d (merge: %s)\n",
           vmas_after_merge, (vmas_after_merge < vmas_after_split) ? "YES" : "NO/partial");

    munmap(p, sz);

    /* WHY: THP status check */
    printf("\n  Transparent Huge Pages (THP) status:\n");
    fp = fopen("/sys/kernel/mm/transparent_hugepage/enabled", "r");
    if (fp) {
        char val[128] = {0};
        if (fgets(val, sizeof(val), fp)) printf("  THP: %s", val);
        fclose(fp);
    } else {
        printf("  /sys/kernel/mm/transparent_hugepage/enabled not found.\n");
    }
    printf("  [always]=auto-promote; [madvise]=only with MADV_HUGEPAGE; [never]=disabled.\n");
}

int main(void) {
    /* EXPECTED OUTPUT (Linux x86_64):
     *   Phase 1: Base VMAs ~15-30; after each mmap +1 (if non-adjacent); after munmap returns
     *   Phase 2: VMAs before mprotect: N; after mprotect(middle): N+2 (split into 3 VMAs)
     *   Phase 3: Parent value=100; Child before write: 100 (COW shared page);
     *            Child after write: 999 (COW triggered); Parent: still 100
     *   Phase 4: New VMA appears in maps; split on mprotect; merge on restore
     *            THP setting from /sys/kernel/mm/transparent_hugepage/enabled
     */
    printf("=== Lab 10: Process Address Spaces Using Paging ===\n");
    demo_vma_growth();
    demo_mprotect_split();
    demo_cow_after_fork();
    demo_maps_before_after_mmap();

    /* === EXERCISE ===
     * Try these hands-on tasks:
     * 1. Use MAP_FIXED to place two adjacent 4K anonymous mappings at a chosen
     *    page-aligned address.  Check /proc/self/maps to see if they appear as one
     *    merged VMA or two.  Then change one's permissions and observe the split.
     *    This demonstrates VMA merge/split rules in the kernel.
     * 2. After fork(), use /proc/self/pagemap (as root) to read the physical frame
     *    numbers for a shared COW page in parent and child BEFORE any write.
     *    They should be identical.  After the child writes, re-read both: they
     *    should differ (child got a private copy via COW).
     * 3. Modern (huge pages in ML workloads): call madvise(p, 512<<20, MADV_HUGEPAGE)
     *    on a 512 MiB anonymous allocation, touch all pages, and check
     *    /proc/self/smaps for the AnonHugePages field.  For ML frameworks,
     *    huge pages reduce TLB miss rate for large weight tensors.  Measure with:
     *      perf stat -e dTLB-load-misses ./lab_10
     *
     * OBSERVE: VMA merging happens automatically when two adjacent VMAs have matching
     *          permissions, flags, and file offset.  This keeps the VMA count low,
     *          which matters because the kernel uses a maple tree (Linux 6.1+) for
     *          O(log n) VMA lookups: fewer VMAs = faster page fault handling.
     * WHY:     MAP_FIXED_NOREPLACE (Linux 4.17+) is preferred over MAP_FIXED for
     *          allocating at specific addresses because it fails safely if something
     *          is already mapped there, preventing silent data corruption.
     */
    printf("\n========== Hands-On Exercise ==========\n");
    printf("1. Use MAP_FIXED to place adjacent 4K mappings; observe merge/split in maps.\n");
    printf("2. As root: read /proc/self/pagemap PFNs before/after COW write in child;\n");
    printf("   verify parent and child share same physical frame until child writes.\n");
    printf("3. Modern (huge pages for ML): call madvise MADV_HUGEPAGE on 512 MiB;\n");
    printf("   check AnonHugePages in smaps; measure dTLB-load-misses with perf.\n");

    printf("\n========== Quiz ==========\n");
    printf("Q1. What kernel data structure tracks VMAs? (hint: maple tree in 6.x, rb-tree before)\n");
    printf("Q2. Why does mprotect on a sub-range split a VMA?\n");
    printf("Q3. Explain the COW sequence: fork -> read -> write -> page fault -> copy.\n");
    printf("Q4. What happens to the parent's page tables when the child does exec()?\n");
    printf("Q5. Why does each shared library (.so) create multiple VMAs (r-x, r--, rw-)?\n");
    printf("Q6. What is the difference between MAP_FIXED and MAP_FIXED_NOREPLACE,\n");
    printf("    and why is MAP_FIXED dangerous for application-level code?\n");
    printf("Q7. How does Transparent Huge Page (THP) promotion work in the kernel --\n");
    printf("    when does khugepaged coalesce 4K pages into a 2 MiB huge page?\n");
    return 0;
}
