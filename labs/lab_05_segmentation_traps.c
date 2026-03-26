/*
 * lab_05_segmentation_traps.c
 *
 * Topic: Segmentation, Trap Handling
 *
 * Demonstrates:
 *   1. SIGSEGV generation by accessing invalid memory
 *   2. Signal handler receiving fault address info
 *   3. mprotect to create guard pages and trap on access
 *   4. Stack overflow detection
 *
 * Build: gcc -O0 -Wall -o lab_05 lab_05_segmentation_traps.c
 *
 * Author: Arun Singh | arunsingh.in@gmail.com
 * OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <ucontext.h>
#include <setjmp.h>
#include <sys/mman.h>
#include <unistd.h>

static sigjmp_buf jump_buf;
static volatile int fault_count = 0;

static void print_section(const char *t) { printf("\n========== %s ==========\n", t); }

static void segv_handler(int sig, siginfo_t *si, void *ctx) {
    fault_count++;
    printf("  [TRAP] Signal %d (%s) at address %p\n", sig,
           sig == SIGSEGV ? "SIGSEGV" : sig == SIGBUS ? "SIGBUS" : "OTHER",
           si->si_addr);
    printf("  [TRAP] Code: %d (%s)\n", si->si_code,
           si->si_code == SEGV_MAPERR ? "MAPERR (unmapped)" :
           si->si_code == SEGV_ACCERR ? "ACCERR (permission)" : "other");

#if defined(__x86_64__)
    ucontext_t *uc = (ucontext_t *)ctx;
    printf("  [TRAP] RIP (faulting instruction): %p\n",
           (void *)uc->uc_mcontext.gregs[REG_RIP]);
#endif

    siglongjmp(jump_buf, 1);
}

static void setup_handler(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = segv_handler;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
}

static void demo_null_deref(void) {
    print_section("Phase 1: NULL Pointer Dereference Trap");
    printf("  Dereferencing NULL generates SIGSEGV — the kernel delivers a trap.\n\n");

    if (sigsetjmp(jump_buf, 1) == 0) {
        volatile int *p = NULL;
        printf("  About to dereference NULL...\n");
        int x = *p;
        (void)x;
        printf("  This should never print.\n");
    } else {
        printf("  Recovered from SIGSEGV via signal handler + longjmp.\n");
    }
}

static void demo_guard_page(void) {
    print_section("Phase 2: Guard Page via mprotect");
    printf("  mprotect(PROT_NONE) creates an inaccessible 'guard page'.\n");
    printf("  Any access triggers SIGSEGV with code SEGV_ACCERR.\n\n");

    size_t ps = (size_t)sysconf(_SC_PAGESIZE);
    void *page = mmap(NULL, ps, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (page == MAP_FAILED) { perror("mmap"); return; }

    /* Write succeeds */
    memset(page, 0xAB, ps);
    printf("  Wrote to page at %p — OK\n", page);

    /* Remove all permissions */
    if (mprotect(page, ps, PROT_NONE) != 0) { perror("mprotect"); return; }
    printf("  mprotect(PROT_NONE) — page is now a guard page\n");

    if (sigsetjmp(jump_buf, 1) == 0) {
        printf("  Attempting to read guard page...\n");
        volatile char c = *(volatile char *)page;
        (void)c;
    } else {
        printf("  Caught SIGSEGV on guard page access — this is how stack overflow is detected!\n");
    }

    munmap(page, ps);
}

static void demo_write_protect(void) {
    print_section("Phase 3: Write-Protection Trap (COW foundation)");
    printf("  mprotect(PROT_READ) makes a page read-only.\n");
    printf("  Writing triggers SIGSEGV — same mechanism as Copy-on-Write.\n\n");

    size_t ps = (size_t)sysconf(_SC_PAGESIZE);
    void *page = mmap(NULL, ps, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    memset(page, 0, ps);

    mprotect(page, ps, PROT_READ);
    printf("  Page at %p is now read-only.\n", page);

    if (sigsetjmp(jump_buf, 1) == 0) {
        printf("  Attempting write...\n");
        *(volatile char *)page = 0xFF;
    } else {
        printf("  Caught SIGSEGV — write to read-only page.\n");
        printf("  The kernel uses this exact mechanism for COW after fork().\n");
    }

    munmap(page, ps);
}

/* WHY: KPTI (Kernel Page Table Isolation) was introduced in Linux 4.15 to mitigate
 *      Meltdown (CVE-2017-5754).  It maintains TWO page table root pointers per process:
 *      one for userspace (only user-page mappings, no kernel mappings) and one for kernel.
 *      On every syscall/interrupt, the CPU switches CR3.  This means every kernel entry
 *      flushes TLB entries not tagged with PCID, adding ~100-200 cycles overhead.
 *      Processors with PCID support amortize this by tagging TLB entries per-ASID.
 *
 * WHY: SIGBUS vs SIGSEGV:
 *      SIGSEGV = virtual address valid but mapping issue (unmapped or permission denied).
 *      SIGBUS  = physical-level error: unaligned access (on strict-alignment arches),
 *                hardware memory error (ECC), or accessing beyond end of mmap'd file
 *                (file was truncated after mmap).  On x86_64 SIGBUS is rare for aligned
 *                accesses but common with file-backed mmap where file size shrinks.
 */

static void demo_bus_error_sim(void) {
    print_section("Phase 4: Simulating SIGBUS via Truncated mmap");
    printf("  SIGBUS occurs when accessing a file-backed page that no longer exists.\n");
    printf("  This happens when a file is truncated after being mmap'd.\n\n");

    /* Create a temp file, write 2 pages, mmap 2 pages, truncate to 0, access page 2 */
    char tmpfile[] = "/tmp/lab05_busXXXXXX";
    int fd = mkstemp(tmpfile);
    if (fd < 0) { perror("mkstemp"); return; }

    size_t ps = (size_t)sysconf(_SC_PAGESIZE);
    char buf[4096];
    memset(buf, 0xAA, sizeof(buf));
    /* Write 2 pages */
    ssize_t w1 = write(fd, buf, ps);
    ssize_t w2 = write(fd, buf, ps);
    (void)w1; (void)w2;

    void *p = mmap(NULL, 2 * ps, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (p == MAP_FAILED) { perror("mmap"); close(fd); unlink(tmpfile); return; }

    /* OBSERVE: Accessing first page is fine */
    volatile char c = *(char *)p;
    printf("  Access page 0 (within file): value=0x%02x  OK\n", (unsigned char)c);

    /* Truncate file to 0 — now the second page is beyond EOF */
    ftruncate(fd, 0);
    printf("  Truncated file to 0 bytes.\n");
    printf("  Accessing page 1 (beyond EOF) should deliver SIGBUS...\n");

    /* WHY: The kernel cannot back the page with file data (file is empty).
     *      It cannot zero-fill (file is shared, not anonymous).
     *      Result: SIGBUS with code BUS_OBJERR (object-specific hardware error). */
    if (sigsetjmp(jump_buf, 1) == 0) {
        volatile char *q = (volatile char *)p + ps; /* second page */
        c = *q;
        (void)c;
        printf("  (No SIGBUS -- kernel may have padded with zeros)\n");
    } else {
        printf("  Caught SIGBUS: file-backed page beyond EOF.\n");
        printf("  SIGBUS code BUS_OBJERR: object-specific hardware error.\n");
    }

    munmap(p, 2 * ps);
    close(fd);
    unlink(tmpfile);
}

int main(void) {
    /* EXPECTED OUTPUT (Linux x86_64):
     *   Phase 1: [TRAP] Signal 11 (SIGSEGV) at address 0x0  code=MAPERR (unmapped)
     *            Recovered from SIGSEGV via signal handler + longjmp.
     *   Phase 2: Wrote to page OK; mprotect(PROT_NONE); [TRAP] SEGV_ACCERR; Caught.
     *   Phase 3: Page read-only; [TRAP] SEGV_ACCERR (write to read-only page); Caught.
     *   Phase 4: Access page 0 OK; truncate; [TRAP] SIGBUS BUS_OBJERR; Caught.
     *   Total faults caught: 4
     */
    printf("=== Lab 05: Segmentation, Trap Handling ===\n");
    setup_handler();
    demo_null_deref();
    demo_guard_page();
    demo_write_protect();
    demo_bus_error_sim();

    printf("\n  Total faults caught: %d\n", fault_count);

    /* === EXERCISE ===
     * Try these hands-on tasks:
     * 1. Modify segv_handler() to print the FULL register context from ucontext_t:
     *    gregs[REG_RSP], gregs[REG_RBP], gregs[REG_RIP], gregs[REG_ERR].
     *    The REG_ERR value is the page fault error code from the CPU:
     *      bit 0 = present (1=protection fault, 0=not-present fault)
     *      bit 1 = write (1=fault on write, 0=fault on read)
     *      bit 2 = user (1=user-mode fault, 0=kernel-mode fault)
     *    Verify that SEGV_MAPERR gives ERR with bit 0=0; SEGV_ACCERR gives bit 0=1.
     * 2. Implement a simple demand-paging allocator in userspace:
     *    mmap a PROT_NONE region, install a SIGSEGV handler, and on each fault
     *    call mprotect(fault_page, PAGE_SIZE, PROT_READ|PROT_WRITE) to enable access.
     *    This simulates what the kernel does for anonymous demand paging.
     * 3. Modern (KPTI / Meltdown mitigation): check if KPTI is enabled:
     *      cat /sys/devices/system/cpu/vulnerabilities/meltdown
     *    If it says "Mitigation: PTI", every trap entry incurs a CR3 switch.
     *    Measure: how many nanoseconds does getpid() (a real syscall) take on this
     *    system vs a system with PTI disabled (nopti boot param on a safe machine)?
     *
     * OBSERVE: After sigsetjmp/siglongjmp, execution continues at the setjmp point.
     *          The signal mask is restored (unlike plain longjmp with setjmp).
     *          Without signal mask restoration, a second fault would not deliver SIGSEGV
     *          because SIGSEGV would be masked from the first handler's invocation.
     * WHY:     The kernel delivers SIGSEGV by adding it to the process's pending signal
     *          set and blocking the same signal during handler execution (unless
     *          SA_NODEFER is set).  siglongjmp restores the sigmask saved by sigsetjmp.
     */
    printf("\n========== Hands-On Exercise ==========\n");
    printf("1. Print full register context (RSP, RBP, RIP, ERR) from ucontext_t;\n");
    printf("   decode ERR bits to distinguish protection vs not-present faults.\n");
    printf("2. Implement userspace demand paging: mmap PROT_NONE + SIGSEGV handler\n");
    printf("   that calls mprotect(fault_page, PAGE_SIZE, PROT_RW) on each fault.\n");
    printf("3. Modern (KPTI): check /sys/devices/system/cpu/vulnerabilities/meltdown;\n");
    printf("   measure syscall latency with vs without PTI mitigation.\n");

    printf("\n========== Quiz ==========\n");
    printf("Q1. What is the difference between SEGV_MAPERR and SEGV_ACCERR?\n");
    printf("Q2. How does the kernel use guard pages for stack overflow detection?\n");
    printf("Q3. Explain how write-protection traps enable Copy-on-Write after fork().\n");
    printf("Q4. What CPU exception does a NULL dereference cause on x86? (hint: #PF)\n");
    printf("Q5. Can a userspace program recover from a segfault? What are the risks?\n");
    printf("Q6. What is the difference between SIGSEGV and SIGBUS?  Give a concrete\n");
    printf("    scenario where each is generated (not null dereference for SIGBUS).\n");
    printf("Q7. What is KPTI and why was it introduced?  How does it affect the cost\n");
    printf("    of every system call on post-2018 x86_64 Linux kernels?\n");
    return 0;
}
