/*
 * lab_37_kernel_arch.c
 * Topic: Microkernel, Exokernel, Multikernel
 * Build: gcc -O0 -Wall -pthread -o lab_37 lab_37_kernel_arch.c
 *
 * Author: Arun Singh | arunsingh.in@gmail.com
 * OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
*/
#include <stdio.h>
static void ps(const char *t){printf("\n========== %s ==========\n",t);}
int main(void){
    printf("=== Lab 37: Microkernel, Exokernel, Multikernel ===\n");
    ps("Phase 1: Monolithic Kernel (Linux)");
    printf("  All services in kernel space: FS, networking, drivers, memory mgmt.\n");
    printf("  Pros: fast (no IPC overhead), shared data structures.\n");
    printf("  Cons: any bug can crash the entire system, large attack surface.\n");
    ps("Phase 2: Microkernel (Minix, seL4, QNX)");
    printf("  Minimal kernel: IPC, scheduling, memory. Everything else in userspace.\n");
    printf("  File server, network server, drivers all run as separate processes.\n");
    printf("  Pros: isolation (driver crash doesnt kill kernel), formally verifiable (seL4).\n");
    printf("  Cons: IPC overhead, complexity of message passing.\n");
    ps("Phase 3: Exokernel");
    printf("  Minimal kernel exposes raw hardware to applications.\n");
    printf("  Each application provides its own OS abstractions (libOS).\n");
    printf("  Pros: maximum flexibility and performance.\n");
    printf("  Modern descendant: Unikernels (run single app as kernel).\n");
    ps("Phase 4: Multikernel (Barrelfish)");
    printf("  Each core runs its own kernel instance. Cores communicate via messages.\n");
    printf("  Treats the machine as a distributed system.\n");
    printf("  Relevant to: NUMA servers, heterogeneous systems (CPU+GPU).\n");
    printf("  GPU DCs: heterogeneous scheduling across CPU/GPU/DPU is multikernel-like.\n");
    /* WHY: eBPF as a safe extensibility mechanism in monolithic kernels (Linux 3.18+):
     *      eBPF solves the "monolithic kernel dilemma": add new functionality safely
     *      without risking system stability.  Key properties:
     *      - Verified: the kernel's BPF verifier proves programs terminate and don't crash
     *      - JIT compiled: near-native performance (comparable to kernel code)
     *      - Sandboxed: cannot access arbitrary kernel memory (only via BPF maps + helpers)
     *      - Hot-reloadable: no reboot required to update policy
     *      eBPF use cases: tracing (bpftrace), networking (XDP, TC), security (LSM, seccomp),
     *      scheduling (sched_ext in 6.12), filesystem I/O (io_uring BPF link).
     *      This gives Linux some microkernel-like extensibility properties without the IPC cost.
     *
     * WHY: /proc/sys/kernel/ exposes kernel tunables:
     *      Many are writable at runtime (no reboot): sched_min_granularity_ns, perf_event_*,
     *      panic, randomize_va_space, etc.  This is the kernel's "sysctl" interface,
     *      implemented via the sysctl_table registered by each subsystem.
     */
    ps("Phase 5: /proc/sys/kernel Entries and eBPF Extensibility");
    printf("  Selected /proc/sys/kernel/ tunables:\n\n");
    {
        const char *tunables[] = {
            "ostype", "osrelease", "version", "hostname",
            "pid_max", "threads-max", "randomize_va_space",
            "panic", "perf_event_paranoid", "kptr_restrict",
            NULL
        };
        for (int i = 0; tunables[i]; i++) {
            char path[128], val[256];
            snprintf(path, sizeof(path), "/proc/sys/kernel/%s", tunables[i]);
            FILE *fp2 = fopen(path, "r");
            if (fp2) {
                if (fgets(val, sizeof(val), fp2)) {
                    val[strcspn(val, "\n")] = '\0';
                    printf("  %-30s = %s\n", tunables[i], val);
                }
                fclose(fp2);
            }
        }
    }

    printf("\n  eBPF as safe monolithic kernel extensibility:\n");
    printf("    Traditional: extend kernel via loadable modules (unsafe, can crash)\n");
    printf("    eBPF: extend via verified BPF programs (safe, hot-reloadable)\n");
    printf("    Verifier ensures: bounded loops, valid memory access, no unbounded stack\n");
    printf("    JIT: BPF programs compiled to native code, ~same speed as kernel functions\n\n");
    printf("  eBPF use cases (2024):\n");
    printf("    bpftrace: dynamic tracing without kernel recompile\n");
    printf("    XDP: packet processing at NIC level (millions of pps per core)\n");
    printf("    sched_ext (6.12): entire CPU scheduler in BPF\n");
    printf("    LSM BPF: security policy in BPF (Cilium, Falco)\n");

    /* Check if BPF is available */
    printf("\n  BPF availability check:\n");
    FILE *fp3 = fopen("/proc/sys/kernel/bpf_stats_enabled", "r");
    if (fp3) {
        char val[8] = {0};
        if (fgets(val, sizeof(val), fp3)) {
            printf("  bpf_stats_enabled: %s", val);
        }
        fclose(fp3);
    }
    fp3 = fopen("/proc/sys/kernel/unprivileged_bpf_disabled", "r");
    if (fp3) {
        char val[8] = {0};
        if (fgets(val, sizeof(val), fp3)) {
            val[strcspn(val, "\n")] = '\0';
            printf("  unprivileged_bpf_disabled: %s (1=only root can load BPF)\n", val);
        }
        fclose(fp3);
    }

    /* === EXERCISE ===
     * Try these hands-on tasks:
     * 1. Read 10 /proc/sys/kernel/ entries and understand their impact.
     *    Try changing randomize_va_space: echo 0 > /proc/sys/kernel/randomize_va_space
     *    Run this program twice -- code/stack/heap addresses should be IDENTICAL.
     *    Restore: echo 2 > /proc/sys/kernel/randomize_va_space (full ASLR)
     * 2. Compare syscall IPC overhead: microkernel IPC requires a context switch per
     *    message (>1 us per round-trip on Linux).  Measure pipe round-trip latency
     *    (similar to microkernel IPC): 100K pipe round-trips, compute ns/round-trip.
     *    This is the overhead penalty of microkernel architectures like seL4 or QNX.
     * 3. Modern (eBPF extensibility): install bpftrace and run:
     *      bpftrace -e 'tracepoint:syscalls:sys_enter_* { @[probe] = count(); }'
     *    This traces ALL syscalls system-wide and counts per type.  Observe which
     *    syscalls are most common on your system.  This is impossible without
     *    kernel modification in a traditional monolithic kernel.
     *
     * OBSERVE: /proc/sys/kernel/pid_max limits total processes on the system.
     *          Default 32768 on 32-bit, 4194304 on 64-bit.  A container runtime
     *          fork-bomb can exhaust pid_max, causing global denial of service.
     *          This is why cgroups v2 has pids.max per-cgroup limit.
     * WHY:     eBPF's key advantage over kernel modules: the BPF verifier PROVES
     *          the program cannot crash the kernel.  A kernel module has no such
     *          guarantee -- a buggy module can corrupt kernel memory.
     */
    printf("\n========== Hands-On Exercise ==========\n");
    printf("1. Change randomize_va_space to 0; run lab twice; verify identical addresses.\n");
    printf("2. Measure pipe round-trip latency (100K iterations) as proxy for\n");
    printf("   microkernel IPC overhead -- compare to function call cost.\n");
    printf("3. Modern (eBPF): use bpftrace to trace all syscalls system-wide;\n");
    printf("   identify top 5 syscalls; demonstrate extensibility without kernel changes.\n");

    printf("\n========== Quiz ==========\n");
    printf("Q1. Why did Linux choose monolithic over microkernel?\n");
    printf("Q2. What is the Tanenbaum-Torvalds debate about?\n");
    printf("Q3. How does seL4 achieve formal verification?\n");
    printf("Q4. Why is the multikernel model relevant to GPU data centres?\n");
    printf("Q5. What is eBPF and how does it give a monolithic kernel some microkernel-like\n");
    printf("    extensibility properties without the IPC overhead?\n");
    printf("Q6. What does the BPF verifier check, and why can a verified BPF program\n");
    printf("    never crash the kernel even without a hardware sandbox?\n");
    printf("Q7. How does sched_ext (Linux 6.12) allow writing a complete CPU scheduler\n");
    printf("    in BPF?  What kernel hook does it expose to BPF programs?\n");
    return 0;
}
