/*
 * lab_16.c
 * Topic: Process kernel stack, scheduler, fork, PCB
 * Build: gcc -O0 -Wall -pthread -o lab_16 lab_16_*.c
 *
 * Author: Arun Singh | arunsingh.in@gmail.com
 * OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sched.h>
#include <sys/resource.h>
static void print_section(const char *t){printf("\n========== %s ==========\n",t);}
int main(void){
    printf("=== Lab 16: Kernel Stack, Scheduler, Fork, PCB ===\n");
    print_section("Phase 1: Scheduler Info from /proc");
    char path[128];
    snprintf(path,sizeof(path),"/proc/%d/sched",getpid());
    FILE *fp=fopen(path,"r");
    if(fp){
        char l[256];int n=0;
        while(fgets(l,sizeof(l),fp)&&n++<15)printf("  %s",l);
        fclose(fp);
    }
    print_section("Phase 2: Scheduling Policy");
    int policy=sched_getscheduler(0);
    printf("  Current policy: %s\n",
        policy==SCHED_OTHER?"SCHED_OTHER (CFS)":
        policy==SCHED_FIFO?"SCHED_FIFO":
        policy==SCHED_RR?"SCHED_RR":"UNKNOWN");
    printf("  Nice value: %d\n",getpriority(PRIO_PROCESS,0));
    printf("  CFS (Completely Fair Scheduler) is default for normal processes.\n");
    printf("  GPU DC workloads may use SCHED_FIFO for latency-sensitive NCCL threads.\n");
    print_section("Phase 3: Kernel Stack Size");
    printf("  Default kernel stack: 8 KiB (x86, THREAD_SIZE) or 16 KiB (some configs)\n");
    printf("  This is much smaller than user stack (typically 8 MiB).\n");
    printf("  Kernel stack overflow = kernel panic (no guard page in traditional design).\n");
    printf("  Kernel 6.x: CONFIG_VMAP_STACK adds guard pages to kernel stacks.\n");
    print_section("Phase 4: PCB (task_struct) Fields");
    printf("  key fields in task_struct:\n");
    printf("    pid, tgid (thread group ID = process PID)\n");
    printf("    state (TASK_RUNNING, TASK_INTERRUPTIBLE, etc.)\n");
    printf("    mm (pointer to mm_struct, NULL for kernel threads)\n");
    printf("    fs (filesystem info), files (open file table)\n");
    printf("    signal (signal handling), stack (kernel stack pointer)\n");
    printf("    sched_entity (CFS scheduling)\n");
    printf("    cpus_mask (CPU affinity)\n");
    /* WHY: task_struct in Linux 6.x has grown to ~9 KB on x86_64 (check with
     *      'pahole -C task_struct vmlinux' from kernel debug package).
     *      Key fields added in 6.x vs 5.x:
     *      - futex_waiters (for fast futex path)
     *      - sched_ext_entity (for BPF-based scheduler, sched_ext, 6.12+)
     *      - mm_cid (per-mm context ID for PCID optimization)
     *      The struct is padded to avoid false sharing between CPUs.
     *
     * WHY: EEVDF (Earliest Eligible Virtual Deadline First) replaced CFS in Linux 6.6.
     *      CFS used virtual runtime (vruntime) to track fairness -- the process with
     *      smallest vruntime runs next.  EEVDF adds a deadline: each process gets a
     *      virtual deadline based on its weight and slice.  The eligible process with
     *      the earliest deadline runs next.  This improves latency for interactive tasks
     *      while maintaining fairness for CPU-bound workloads.
     */
    print_section("Phase 5: Reading /proc/self/sched and /proc/self/status");
    {
        /* Read key scheduler fields */
        char path[128];
        snprintf(path, sizeof(path), "/proc/%d/sched", getpid());
        FILE *fp = fopen(path, "r");
        if (fp) {
            char l[256]; int n = 0;
            printf("  /proc/self/sched key fields:\n");
            while (fgets(l, sizeof(l), fp) && n < 20) {
                /* Show the most educational fields */
                if (strstr(l,"se.vruntime") || strstr(l,"nr_switches") ||
                    strstr(l,"prio") || strstr(l,"wait_sum") ||
                    strstr(l,"exec_max") || strstr(l,"sum_exec_runtime") ||
                    strstr(l,"se.nr_migrations") || l[0] == '(')
                    printf("    %s", l);
                n++;
            }
            fclose(fp);
        } else {
            printf("  /proc/self/sched: not available (need kernel with CONFIG_SCHED_DEBUG)\n");
        }

        /* OBSERVE: se.vruntime increases as the process runs.  Higher vruntime = more
         *          CPU time used = lower scheduler priority under CFS/EEVDF. */
        snprintf(path, sizeof(path), "/proc/%d/status", getpid());
        fp = fopen(path, "r");
        if (fp) {
            char l[256];
            printf("\n  /proc/self/status scheduler-related fields:\n");
            while (fgets(l, sizeof(l), fp)) {
                if (strncmp(l,"voluntary",9)==0 || strncmp(l,"nonvoluntary",12)==0 ||
                    strncmp(l,"Threads:",8)==0 || strncmp(l,"SigCgt:",7)==0)
                    printf("    %s", l);
            }
            fclose(fp);
        }
    }

    /* WHY: sched_ext (BPF-programmable scheduler, Linux 6.12+) lets you write a
     *      complete CPU scheduler in BPF (verified safe eBPF programs).
     *      This enables custom scheduling policies (e.g., deadline-aware GPU kernel
     *      dispatch, NUMA-aware tensor parallel scheduling) without kernel patches. */
    printf("\n  Modern: EEVDF replaced CFS in Linux 6.6 for better latency.\n");
    printf("  sched_ext (Linux 6.12): write custom schedulers in BPF -- no kernel patch needed.\n");

    /* === EXERCISE ===
     * Try these hands-on tasks:
     * 1. Read /proc/self/sched and extract se.vruntime.  Run a CPU-bound loop for
     *    100ms, then re-read vruntime.  Compute the increase.  Divide by 100ms to get
     *    the effective vruntime rate.  Compare two processes with nice 0 vs nice 10.
     * 2. Read /proc/self/status voluntary_ctxt_switches before and after a mutex
     *    lock/unlock cycle where another thread holds the lock.  Observe the counter
     *    increment -- each block on a mutex is a voluntary context switch.
     * 3. Modern (task_struct in Linux 6.x): install kernel debug symbols and run:
     *      pahole -C task_struct /usr/lib/debug/boot/vmlinux-$(uname -r)
     *    Note the struct size (~9 KB) and the se (sched_entity) sub-struct.
     *    Identify: se.vruntime, se.deadline (EEVDF field added in 6.6),
     *    and sched_ext_entity (if CONFIG_SCHED_CLASS_EXT is enabled, 6.12+).
     *
     * OBSERVE: vruntime increases even when the process is sleeping -- it is normalized
     *          by the scheduler period so sleeping processes do not accumulate "debt".
     *          Under EEVDF, a process that sleeps wakes up with a fresh deadline,
     *          getting a burst of CPU time (latency_nice optimization).
     * WHY:     The kernel stack is small (8-16 KiB) because each runnable thread in the
     *          kernel consumes its stack until it sleeps.  With 10000 threads, 8 KiB
     *          each = 80 MiB just for stacks.  With VMAP_STACK, guard pages prevent
     *          silent overflow by ensuring a page fault on stack overflow.
     */
    printf("\n========== Hands-On Exercise ==========\n");
    printf("1. Read se.vruntime from /proc/self/sched before and after a CPU-bound loop;\n");
    printf("   compare vruntime rate for nice 0 vs nice 10 processes.\n");
    printf("2. Read voluntary_ctxt_switches before/after acquiring a contended mutex;\n");
    printf("   observe each block increments the voluntary switch counter.\n");
    printf("3. Modern (Linux 6.x task_struct): use 'pahole -C task_struct vmlinux';\n");
    printf("   find se.vruntime, se.deadline (EEVDF), and sched_ext_entity fields.\n");

    printf("\n========== Quiz ==========\n");
    printf("Q1. What is the difference between PID and TGID in the kernel?\n");
    printf("Q2. Why is the kernel stack so much smaller than user stack?\n");
    printf("Q3. What does CFS guarantee that SCHED_FIFO does not?\n");
    printf("Q4. What happens if a kernel function overflows the kernel stack?\n");
    printf("Q5. What is EEVDF and how does it differ from CFS in terms of\n");
    printf("    the scheduling decision criterion (vruntime vs deadline)?\n");
    printf("Q6. What is sched_ext (Linux 6.12+) and why is BPF-based scheduling\n");
    printf("    useful for GPU datacenter workloads?\n");
    printf("Q7. What fields in task_struct distinguish a kernel thread from a user\n");
    printf("    process (hint: mm pointer and flags like PF_KTHREAD)?\n");
    return 0;
}
