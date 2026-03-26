/*
 * lab_06_trap_handlers.c
 * Topic: Traps, Trap Handlers
 * Demonstrates signal dispatch, sigaction, fault info, FPE, illegal instruction.
 * Build: gcc -O0 -Wall -o lab_06 lab_06_trap_handlers.c
 *
 * Author: Arun Singh | arunsingh.in@gmail.com
 * OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
*/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <setjmp.h>
#include <ucontext.h>
#include <fenv.h>
#include <math.h>
#include <unistd.h>

static sigjmp_buf jbuf;
static void print_section(const char *t) { printf("\n========== %s ==========\n", t); }

static void trap_handler(int sig, siginfo_t *si, void *ctx) {
    const char *name = sig == SIGFPE ? "SIGFPE" : sig == SIGILL ? "SIGILL" :
                       sig == SIGSEGV ? "SIGSEGV" : sig == SIGBUS ? "SIGBUS" :
                       sig == SIGTRAP ? "SIGTRAP" : "UNKNOWN";
    printf("  [TRAP] %s (signal %d)\n", name, sig);
    printf("  [TRAP] si_code=%d  si_addr=%p\n", si->si_code, si->si_addr);
#if defined(__x86_64__)
    ucontext_t *uc = (ucontext_t *)ctx;
    printf("  [TRAP] RIP=%p  RSP=%p\n",
           (void *)uc->uc_mcontext.gregs[REG_RIP],
           (void *)uc->uc_mcontext.gregs[REG_RSP]);
#endif
    siglongjmp(jbuf, sig);
}

static void install_handlers(void) {
    struct sigaction sa = {0};
    sa.sa_sigaction = trap_handler;
    sa.sa_flags = SA_SIGINFO;
    int sigs[] = {SIGFPE, SIGILL, SIGSEGV, SIGBUS, SIGTRAP, 0};
    for (int i = 0; sigs[i]; i++) sigaction(sigs[i], &sa, NULL);
}

static void demo_div_zero(void) {
    print_section("Phase 1: Division by Zero (SIGFPE)");
    feenableexcept(FE_DIVBYZERO);
    int recovered = sigsetjmp(jbuf, 1);
    if (recovered == 0) {
        volatile int a = 42, b = 0;
        printf("  Computing %d / %d...\n", a, b);
        volatile int c = a / b;
        (void)c;
    } else {
        printf("  Recovered from SIGFPE (signal %d)\n", recovered);
    }
}

static void demo_sigaction_info(void) {
    print_section("Phase 2: Signal Delivery Path");
    printf("  When a trap occurs:\n");
    printf("  1. CPU raises exception (e.g. #DE for div-by-zero, #PF for page fault)\n");
    printf("  2. IDT (Interrupt Descriptor Table) routes to kernel handler\n");
    printf("  3. Kernel checks: is this a user-mode fault?\n");
    printf("  4. Kernel delivers signal to the process\n");
    printf("  5. If sigaction handler is installed, kernel sets up signal frame on user stack\n");
    printf("  6. User handler runs with siginfo_t containing fault details\n");
    printf("  7. Handler returns or longjmps\n\n");

    printf("  Common traps mapped to signals:\n");
    printf("    #DE (Divide Error)      -> SIGFPE\n");
    printf("    #PF (Page Fault)        -> SIGSEGV\n");
    printf("    #UD (Undefined Opcode)  -> SIGILL\n");
    printf("    #BP (Breakpoint / int3) -> SIGTRAP\n");
    printf("    #GP (General Protect)   -> SIGSEGV\n");
}

static void demo_breakpoint(void) {
    print_section("Phase 3: Software Breakpoint (SIGTRAP)");
#if defined(__x86_64__)
    int recovered = sigsetjmp(jbuf, 1);
    if (recovered == 0) {
        printf("  Executing int3 (software breakpoint)...\n");
        __asm__ volatile("int3");
    } else {
        printf("  Caught SIGTRAP — this is how debuggers (gdb) set breakpoints.\n");
    }
#else
    printf("  (x86-specific — skipped)\n");
#endif
}

int main(void) {
    printf("=== Lab 06: Traps, Trap Handlers ===\n");
    install_handlers();
    demo_div_zero();
    demo_sigaction_info();
    demo_breakpoint();
    printf("\n========== Quiz ==========\n");
    printf("Q1. Trace the path from a division-by-zero in user code to the signal handler.\n");
    printf("Q2. What is the IDT and what role does it play in trap handling?\n");
    printf("Q3. How does gdb use INT3 to implement breakpoints?\n");
    printf("Q4. Why does the kernel set up a 'signal frame' on the user stack?\n");
    printf("Q5. Can a kernel thread receive SIGSEGV? Why or why not?\n");
    return 0;
}
