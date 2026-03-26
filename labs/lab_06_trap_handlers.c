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

/* WHY: Trap vs Interrupt vs Exception (x86 terminology):
 *      - Exception: synchronous CPU event caused by the currently executing instruction
 *        (e.g., #PF page fault, #DE div-by-zero, #UD undefined opcode).
 *        Can be faults (restartable), traps (next instruction), or aborts (fatal).
 *      - Interrupt: asynchronous event from hardware (NIC, disk, timer) via LAPIC/IOAPIC.
 *        The CPU finishes the current instruction, then handles the interrupt.
 *      - Software trap: deliberate INT n instruction (INT3=breakpoint, INT 0x80=old syscall).
 *      In Linux, the kernel's IDT handlers for faults deliver signals to userspace.
 *
 * WHY: Intel SGX (Software Guard Extensions) enclaves run in a protected execution
 *      environment.  Hardware exceptions INSIDE an enclave cause an Asynchronous Enclave
 *      Exit (AEX).  The CPU saves enclave state into a State Save Area (SSA), exits to
 *      an untrusted handler (the host OS).  The host OS cannot read enclave memory.
 *      Linux SGX driver (merged in 5.11) handles AEX and re-enters the enclave via ERESUME.
 */

static void demo_fault_address_handler(void) {
    print_section("Phase 4: Enhanced Handler: Fault Address and Signal Type");
    printf("  A production-quality handler should identify fault type and address.\n\n");

    /* OBSERVE: When SIGSEGV is delivered with SA_SIGINFO, si->si_addr is the
     *          faulting virtual address.  si->si_code distinguishes MAPERR vs ACCERR.
     *          RIP points to the instruction that caused the fault (for SEGV, #PF is a fault
     *          class, so RIP points to the faulting instruction, not the next one). */

    /* Demonstrate reading si_addr from the handler already installed */
    if (sigsetjmp(jbuf, 1) == 0) {
        /* Access unmapped address 0xDEADBEEF0000 */
        volatile char *bad = (volatile char *)0xDEADBEEF0000UL;
        printf("  Accessing unmapped address %p...\n", (void *)bad);
        char c = *bad;
        (void)c;
    } else {
        printf("  Handler printed si_addr (fault address) and RIP above.\n");
        printf("  OBSERVE: si_addr matches the bad pointer we accessed.\n");
        printf("  WHY: #PF error code bit 0 = 0 (not-present), bit 1 = 0 (read).\n");
    }
}

int main(void) {
    /* EXPECTED OUTPUT (Linux x86_64):
     *   Phase 1: [TRAP] SIGFPE (signal 8)  si_code=1  RIP=<addr of div instruction>
     *            Recovered from SIGFPE (signal 8)
     *   Phase 2: Signal delivery path printed (educational, no live faults)
     *   Phase 3: [TRAP] SIGTRAP  si_addr=<int3 addr>  Caught SIGTRAP
     *   Phase 4: [TRAP] SIGSEGV  si_code=1 (MAPERR)  si_addr=0xdeadbeef0000
     *            RIP points to the load instruction attempting the access
     */
    printf("=== Lab 06: Traps, Trap Handlers ===\n");
    install_handlers();
    demo_div_zero();
    demo_sigaction_info();
    demo_breakpoint();
    demo_fault_address_handler();

    /* === EXERCISE ===
     * Try these hands-on tasks:
     * 1. Install a SIGBUS handler and generate a SIGBUS by:
     *    a) mmap a file, then ftruncate the file to 0, then access the mmap'd region.
     *    b) In the handler, print si->si_code -- it should be BUS_OBJERR (3).
     *    Compare: SIGSEGV si_code is SEGV_MAPERR(1) or SEGV_ACCERR(2).
     * 2. From the trap handler's ucontext_t, decode the #PF error code (REG_ERR):
     *      bit 0 = P (1=protection fault, 0=not-present)
     *      bit 1 = W (1=write fault, 0=read fault)
     *      bit 2 = U (1=user mode, 0=kernel mode)
     *      bit 3 = R (1=reserved bit in PTE was set)
     *    Print a human-readable description of each fault you trigger.
     * 3. Modern (SGX enclaves): read about Asynchronous Enclave Exit (AEX) in the
     *    Intel SGX developer guide.  When an enclave takes a #PF, the CPU performs AEX:
     *    saves enclave registers to SSA (State Save Area), exits to host OS handler.
     *    The Linux SGX driver (/dev/sgx_enclave) receives the AEX and can choose to
     *    re-enter the enclave (ERESUME) or kill it.  How is this similar to normal
     *    SIGSEGV delivery?  How is it different (hint: enclave memory is encrypted)?
     *
     * OBSERVE: Trap (INT3) delivers SIGTRAP and RIP points to the byte AFTER int3.
     *          Fault (#PF, #DE) delivers SIGSEGV/SIGFPE and RIP points to the
     *          faulting instruction itself -- so the kernel can restart it after fix-up.
     * WHY:     The distinction between fault-class and trap-class exceptions matters
     *          for ptrace-based debuggers: for breakpoints (INT3, trap class), gdb must
     *          decrement the IP before single-stepping so execution restarts correctly.
     */
    printf("\n========== Hands-On Exercise ==========\n");
    printf("1. Install SIGBUS handler; trigger via truncated mmap; print BUS_OBJERR code.\n");
    printf("2. Decode #PF error code from REG_ERR in ucontext_t for each fault type.\n");
    printf("3. Modern (SGX): research Asynchronous Enclave Exit (AEX) -- how does the\n");
    printf("   Linux SGX driver handle hardware exceptions inside an enclave?\n");

    printf("\n========== Quiz ==========\n");
    printf("Q1. Trace the path from a division-by-zero in user code to the signal handler.\n");
    printf("Q2. What is the IDT and what role does it play in trap handling?\n");
    printf("Q3. How does gdb use INT3 to implement breakpoints?\n");
    printf("Q4. Why does the kernel set up a 'signal frame' on the user stack?\n");
    printf("Q5. Can a kernel thread receive SIGSEGV? Why or why not?\n");
    printf("Q6. What is the difference between a trap, an interrupt, and a fault on x86?\n");
    printf("    Give one example CPU exception of each class.\n");
    printf("Q7. How does Intel SGX handle hardware exceptions inside an enclave (AEX)?\n");
    printf("    Why can the host OS not simply read the enclave's register state?\n");
    return 0;
}
