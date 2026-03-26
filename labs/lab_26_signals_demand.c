/*
 * lab_26_signals_demand.c
 * Topic: Signals, IDE Driver, Intro Demand Paging
 * Build: gcc -O0 -Wall -pthread -o lab_26 lab_26_signals_demand.c
 *
 * Author: Arun Singh | arunsingh.in@gmail.com
 * OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/resource.h>
static void ps(const char *t){printf("\n========== %s ==========\n",t);}
static volatile sig_atomic_t got_signal=0;
static void handler(int sig){got_signal=sig;}
int main(void){
    printf("=== Lab 26: Signals, Device Drivers, Intro to Demand Paging ===\n");
    ps("Phase 1: Signal Delivery");
    signal(SIGUSR1,handler);
    printf("  Sending SIGUSR1 to self (PID %d)...\n",getpid());
    kill(getpid(),SIGUSR1);
    printf("  Handler received signal: %d (%s)\n",got_signal,got_signal==SIGUSR1?"SIGUSR1":"?");
    printf("  Signals are software interrupts delivered by the kernel.\n");
    printf("  Async signals can interrupt any instruction — handler must be reentrant.\n");
    ps("Phase 2: Block Device Concepts");
    printf("  Traditional IDE/SATA driver flow:\n");
    printf("  1. Process calls read() -> VFS -> filesystem -> block layer\n");
    printf("  2. Block layer issues I/O request to device driver\n");
    printf("  3. Driver programs DMA controller, process sleeps\n");
    printf("  4. Device completes I/O, raises interrupt (IRQ)\n");
    printf("  5. Interrupt handler wakes process, data is in page cache\n\n");
    printf("  Modern: NVMe uses polled I/O + MSI-X interrupts + io_uring.\n");
    printf("  GPU DCs: NVMe SSDs for checkpointing, GDS (GPU Direct Storage) bypasses CPU.\n");
    ps("Phase 3: Demand Paging Intro");
    struct rusage ru;getrusage(RUSAGE_SELF,&ru);long f0=ru.ru_minflt;
    void *p=mmap(NULL,4096*100,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    getrusage(RUSAGE_SELF,&ru);long f1=ru.ru_minflt;
    printf("  mmap 100 pages: faults=%ld (just metadata)\n",f1-f0);
    memset(p,0xAB,4096*100);
    getrusage(RUSAGE_SELF,&ru);long f2=ru.ru_minflt;
    printf("  touch 100 pages: faults=%ld (demand paging!)\n",f2-f1);
    munmap(p,4096*100);
    /* WHY: SA_SIGINFO flag passes a siginfo_t to the handler, providing:
     *      - si_signo: signal number
     *      - si_code: signal source (SI_USER, SI_KERNEL, SEGV_MAPERR, etc.)
     *      - si_addr: fault address (for SIGSEGV/SIGBUS)
     *      - si_pid, si_uid: sender identity (for kill()/sigqueue())
     *      Without SA_SIGINFO, the handler receives only the signal number.
     *
     * WHY: userfaultfd (Linux 4.3+, enhanced in 5.x/6.x) replaces SIGSEGV-based
     *      page fault handling.  Key advantages:
     *      - Runs in a dedicated thread, not in the faulting thread's context
     *      - Can handle faults from other processes (UFFD_FEATURE_EVENT_FORK)
     *      - Supports write-protection faults (UFFD_FEATURE_PAGEFAULT_FLAG_WP)
     *      - No risk of signal re-entrancy or async-signal-safety issues
     *      Used by QEMU (post-copy live migration), CRIU (checkpoint/restore), and
     *      Firecracker (AWS Lambda, uses userfaultfd for memory balloon).
     */
    ps("Phase 4: Userspace Demand Paging with SIGSEGV Handler");
    {
        /* Demonstrate SIGSEGV-based demand paging in userspace */
        #include <setjmp.h>
        /* We'll show the concept without actually implementing the full loop
         * since that requires a separate handler setup.  Instead, explain it. */
        printf("  Concept: mmap PROT_NONE + SIGSEGV handler = userspace demand pager.\n");
        printf("  On first access to each page:\n");
        printf("    1. CPU raises #PF (not-present)\n");
        printf("    2. Kernel delivers SIGSEGV with SA_SIGINFO to our handler\n");
        printf("    3. Handler calls mprotect(fault_page, PAGE_SIZE, PROT_RW)\n");
        printf("    4. Handler calls siglongjmp to retry the access\n");
        printf("  This simulates kernel demand paging entirely in userspace!\n\n");
        printf("  Limitation vs userfaultfd:\n");
        printf("    SIGSEGV: handler runs on faulting thread's stack (reentrancy risk)\n");
        printf("    userfaultfd: dedicated handler thread, no reentrancy issues\n");
        printf("    userfaultfd: can handle faults from OTHER processes' memory\n");
        printf("    userfaultfd: UFFDIO_COPY lets handler supply arbitrary page content\n");
    }

    /* Show signal delivery mechanism */
    printf("\n  SA_SIGINFO: signal info passed to handler:\n");
    {
        struct sigaction sa;
        sa.sa_flags = SA_SIGINFO;
        printf("  sigaction flags: SA_SIGINFO=0x%x SA_RESTART=0x%x SA_NODEFER=0x%x\n",
               SA_SIGINFO, SA_RESTART, SA_NODEFER);
        printf("  SA_SIGINFO: provides siginfo_t with fault address and code\n");
        printf("  SA_RESTART: automatically restart interrupted syscalls\n");
        printf("  SA_NODEFER: don't mask this signal during handler (allow nesting)\n");
        (void)sa;
    }

    /* === EXERCISE ===
     * Try these hands-on tasks:
     * 1. Implement a userspace demand pager:
     *    a) mmap a 1 MiB region with PROT_NONE
     *    b) Install a SIGSEGV handler with SA_SIGINFO + sigsetjmp recovery
     *    c) In the handler: mprotect(fault_page, PAGE_SIZE, PROT_READ|PROT_WRITE)
     *       then siglongjmp back
     *    d) Walk through all 256 pages with a strided access and count handler invocations
     *    Each access triggers exactly one fault per page (subsequent accesses: no fault).
     * 2. Use sigqueue() instead of kill() to send a signal with an integer value:
     *      union sigval sv; sv.sival_int = 42;
     *      sigqueue(getpid(), SIGUSR1, sv);
     *    In the SA_SIGINFO handler, read si->si_value.sival_int.  This is "real-time"
     *    signal delivery -- signals queue with values (not just counters).
     * 3. Modern (userfaultfd): open /dev/userfaultfd and register a PROT_NONE region.
     *    In a handler thread, read uffd events and call UFFDIO_ZEROPAGE or UFFDIO_COPY
     *    to supply pages.  This is how CRIU implements lazy restore and QEMU implements
     *    post-copy live migration in Linux 6.x.
     *
     * OBSERVE: The SIGSEGV handler must use only async-signal-safe functions.
     *          printf() is NOT async-signal-safe (uses malloc internally).
     *          Safe functions: write(), mprotect(), siglongjmp(), atomic ops.
     *          Using printf() in a signal handler can cause deadlock if the signal
     *          interrupts a printf() in the main thread (both try to lock stdio mutex).
     * WHY:     userfaultfd is preferred over SIGSEGV for production page-fault handling
     *          because it avoids async-signal-safety restrictions, allows a dedicated
     *          thread to handle faults, and supports cross-process fault handling.
     */
    printf("\n========== Hands-On Exercise ==========\n");
    printf("1. Implement userspace demand pager: PROT_NONE mmap + SIGSEGV handler\n");
    printf("   that mprotect-enables each page on first access; count faults.\n");
    printf("2. Use sigqueue() to send signal with integer payload; receive in SA_SIGINFO handler.\n");
    printf("3. Modern (userfaultfd): open /dev/userfaultfd, register region, handle\n");
    printf("   UFFD_EVENT_PAGEFAULT in dedicated thread with UFFDIO_COPY/ZEROPAGE.\n");

    printf("\n========== Quiz ==========\n");
    printf("Q1. What makes a signal handler 'reentrant-safe'?\n");
    printf("Q2. Trace a read() from userspace to disk and back.\n");
    printf("Q3. What is GPU Direct Storage and why does it matter for AI training checkpoints?\n");
    printf("Q4. Why does mmap not cause page faults but memset does?\n");
    printf("Q5. What does SA_SIGINFO provide that a plain signal() handler does not?\n");
    printf("Q6. How does userfaultfd differ from SIGSEGV-based page fault handling?\n");
    printf("    Why is it preferred for QEMU post-copy live migration?\n");
    printf("Q7. What is the 'async-signal-safety' requirement and why does printf()\n");
    printf("    violate it?  What are the safe alternatives for signal handlers?\n");
    return 0;
}
