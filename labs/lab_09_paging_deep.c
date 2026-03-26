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

/* WHY: madvise(MADV_DONTNEED) tells the kernel it may reclaim the backing pages
 *      for a range of anonymous memory.  The virtual mapping stays intact but the
 *      PTEs are cleared (Present=0).  RSS drops immediately; the next access
 *      triggers a fresh minor fault and the page is zero-filled again.
 *      This is how jemalloc and tcmalloc return memory to the OS without munmap().
 *
 * WHY: userfaultfd (Linux 4.3+) lets a userspace thread handle page faults for a
 *      nominated address range.  UFFD_FEATURE_PAGEFAULT_FLAG_WP enables write-
 *      protection faults.  Live migration tools (QEMU, CRIU) use this to copy
 *      dirty pages while the process keeps running — no kernel changes needed.
 */

static void demo_madvise_dontneed(void) {
    print_section("Phase 4: madvise(MADV_DONTNEED) — Returning Pages to Kernel");

    size_t sz = 8 * 1024 * 1024; /* 8 MiB */
    void *p = mmap(NULL, sz, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) { perror("mmap"); return; }

    /* OBSERVE: After memset the pages are present (Dirty=1, Present=1 in PTEs).
     *          RSS will show ~8 MiB in /proc/self/smaps for this region. */
    memset(p, 0xAB, sz);

    /* Read RSS before MADV_DONTNEED */
    long rss_before = 0;
    {
        FILE *fp = fopen("/proc/self/status", "r");
        char line[256];
        if (fp) {
            while (fgets(line, sizeof(line), fp)) {
                if (strncmp(line, "VmRSS:", 6) == 0) {
                    sscanf(line + 6, "%ld", &rss_before);
                    break;
                }
            }
            fclose(fp);
        }
    }
    printf("  RSS before MADV_DONTNEED: %ld kB\n", rss_before);

    /* WHY: MADV_DONTNEED clears PTEs for the range.  The kernel can immediately
     *      reclaim those physical frames (add them back to the free list).
     *      The VMA itself is not removed — the address range remains valid. */
    if (madvise(p, sz, MADV_DONTNEED) != 0) {
        perror("madvise");
    }

    /* Read RSS after MADV_DONTNEED */
    long rss_after = 0;
    {
        FILE *fp = fopen("/proc/self/status", "r");
        char line[256];
        if (fp) {
            while (fgets(line, sizeof(line), fp)) {
                if (strncmp(line, "VmRSS:", 6) == 0) {
                    sscanf(line + 6, "%ld", &rss_after);
                    break;
                }
            }
            fclose(fp);
        }
    }
    printf("  RSS after  MADV_DONTNEED: %ld kB  (dropped ~%ld kB)\n",
           rss_after, rss_before - rss_after);

    /* OBSERVE: Re-touching the range causes fresh minor faults (zero pages). */
    long min0, maj0, min1, maj1;
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru); min0 = ru.ru_minflt; maj0 = ru.ru_majflt;
    volatile char *q = (volatile char *)p;
    for (size_t i = 0; i < sz; i += (size_t)sysconf(_SC_PAGESIZE)) q[i] = 0;
    getrusage(RUSAGE_SELF, &ru); min1 = ru.ru_minflt; maj1 = ru.ru_majflt;
    printf("  Re-touch after DONTNEED: minor_faults=%ld  major_faults=%ld\n",
           min1-min0, maj1-maj0);
    printf("  (Pages are zero-filled again — content lost after DONTNEED)\n");

    munmap(p, sz);
}

int main(void) {
    /* EXPECTED OUTPUT (Linux x86_64):
     *   === Lab 09: Paging (Deep Dive) ===
     *   --- Phase 1: PTE flags listed (educational, no live output) ---
     *   After mmap(16MiB):    minor_faults=0 or very small (no pages touched yet)
     *   After memset(16MiB):  minor_faults=~4096  (16MiB / 4096B per page)
     *   After 2nd memset:     minor_faults=0  (pages already present, no fault)
     *   --- Phase 3: smaps entry for 4 MiB anonymous region ---
     *   Size: 4096 kB, Rss: 4096 kB, Pss: 4096 kB, Anonymous: 4096 kB
     *   --- Phase 4: MADV_DONTNEED ---
     *   RSS before: ~<some value>+8192 kB; RSS after: drops ~8192 kB
     *   Re-touch faults: ~2048 minor faults (8MiB / 4096B)
     */
    printf("=== Lab 09: Paging (Deep Dive) ===\n");
    demo_pte_flags();
    demo_fault_counting();
    demo_smaps();
    demo_madvise_dontneed();

    /* === EXERCISE ===
     * Try these hands-on tasks:
     * 1. After mmap() but BEFORE any memset, read /proc/self/smaps for that region.
     *    Check "Rss:" — it should be 0 (no page faults yet).  Then touch one byte
     *    per page (loop with stride 4096) and re-read smaps.  Watch Rss grow one
     *    page at a time.  This demonstrates pure demand paging.
     * 2. Use mmap(MAP_SHARED | MAP_ANONYMOUS) for a 4 MiB region, fork(), and have
     *    the child write to it.  In the parent, read the written values.  Contrast
     *    with MAP_PRIVATE to see COW vs truly shared memory.  Check smaps Shared_Dirty
     *    vs Private_Dirty fields to confirm.
     * 3. Modern (userfaultfd for live migration): open /dev/userfaultfd (or use
     *    syscall(__NR_userfaultfd)), register a PROT_NONE region, and in a separate
     *    thread handle UFFD_EVENT_PAGEFAULT by calling UFFDIO_COPY to supply pages.
     *    This mirrors how QEMU implements post-copy live migration and CRIU implements
     *    lazy restore in Linux 6.x.
     *
     * OBSERVE: After MADV_DONTNEED, the content is GONE — next read returns zeros.
     *          After MADV_FREE (Linux 4.5+) the content MAY survive if the kernel
     *          has not reclaimed the page yet (lazy free).
     * WHY:     DONTNEED is the mechanism jemalloc uses for madvise-based purging.
     *          Every call to free() that releases a large chunk eventually calls
     *          madvise(MADV_DONTNEED) so that RSS tracks actual usage.
     */
    printf("\n========== Hands-On Exercise ==========\n");
    printf("1. Read /proc/self/smaps before and after touching mmap'd pages;\n");
    printf("   watch Rss grow page-by-page demonstrating demand paging.\n");
    printf("2. Compare MAP_PRIVATE vs MAP_SHARED after fork(); check smaps\n");
    printf("   Shared_Dirty vs Private_Dirty to confirm COW behavior.\n");
    printf("3. Modern: implement a userfaultfd handler (UFFDIO_COPY) that supplies\n");
    printf("   pages on demand, simulating QEMU post-copy live migration.\n");

    printf("\n========== Quiz ==========\n");
    printf("Q1. What PTE flag change implements Copy-on-Write?\n");
    printf("Q2. Why does the second memset cause near-zero faults?\n");
    printf("Q3. What is the 'Dirty' bit used for by the kernel?\n");
    printf("Q4. What is PSS (Proportional Set Size) and why is it more useful than RSS for shared libs?\n");
    printf("Q5. How does NX bit prevent buffer overflow exploits?\n");
    printf("Q6. What is the difference between MADV_DONTNEED and MADV_FREE?\n");
    printf("    Which one does jemalloc prefer and why?\n");
    printf("Q7. How does userfaultfd differ from SIGSEGV-based page fault handling,\n");
    printf("    and why is it preferred for live migration in modern kernels (6.x)?\n");
    return 0;
}
