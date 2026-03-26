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

int main(void) {
    printf("=== Lab 05: Segmentation, Trap Handling ===\n");
    setup_handler();
    demo_null_deref();
    demo_guard_page();
    demo_write_protect();

    printf("\n  Total faults caught: %d\n", fault_count);
    printf("\n========== Quiz ==========\n");
    printf("Q1. What is the difference between SEGV_MAPERR and SEGV_ACCERR?\n");
    printf("Q2. How does the kernel use guard pages for stack overflow detection?\n");
    printf("Q3. Explain how write-protection traps enable Copy-on-Write after fork().\n");
    printf("Q4. What CPU exception does a NULL dereference cause on x86? (hint: #PF)\n");
    printf("Q5. Can a userspace program recover from a segfault? What are the risks?\n");
    return 0;
}
