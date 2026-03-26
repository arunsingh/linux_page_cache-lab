/*
 * lab_35_scheduling.c
 * Topic: Scheduling Policies
 * Build: gcc -O0 -Wall -pthread -o lab_35 lab_35_scheduling.c
 *
 * Author: Arun Singh | arunsingh.in@gmail.com
 * OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
*/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <sys/resource.h>
static void ps(const char *t){printf("\n========== %s ==========\n",t);}
int main(void){
    printf("=== Lab 35: Scheduling Policies ===\n");
    ps("Phase 1: CFS (Completely Fair Scheduler)");
    printf("  Default scheduler for SCHED_OTHER/SCHED_NORMAL tasks.\n");
    printf("  Uses virtual runtime (vruntime) to ensure fairness.\n");
    printf("  Stored in red-black tree ordered by vruntime.\n");
    printf("  Nice values -20 to +19 adjust weight (lower nice = more CPU).\n\n");
    printf("  Current nice: %d\n",getpriority(PRIO_PROCESS,0));
    printf("  Timeslice target: /proc/sys/kernel/sched_min_granularity_ns\n");
    FILE *fp=fopen("/proc/sys/kernel/sched_min_granularity_ns","r");
    if(fp){char l[64];if(fgets(l,sizeof(l),fp))printf("  sched_min_granularity: %s",l);fclose(fp);}
    ps("Phase 2: Real-Time Schedulers");
    printf("  SCHED_FIFO: first-in first-out, no timeslice, runs until yield/block.\n");
    printf("  SCHED_RR:   round-robin with fixed timeslice among same-priority tasks.\n");
    printf("  Priority: 1-99 (higher = more urgent). Always preempts CFS tasks.\n");
    printf("  GPU DCs: NCCL communication threads often use SCHED_FIFO.\n");
    ps("Phase 3: SCHED_DEADLINE");
    printf("  Earliest Deadline First (EDF) scheduling.\n");
    printf("  Parameters: runtime, deadline, period.\n");
    printf("  Guarantees: if runtime <= deadline, task will complete on time.\n");
    printf("  Used for: real-time audio/video, robotics, latency-critical GPU work.\n");
    ps("Phase 4: CPU Affinity");
    cpu_set_t mask;
    CPU_ZERO(&mask);
    sched_getaffinity(0,sizeof(mask),&mask);
    printf("  This process can run on CPUs:");
    for(int i=0;i<CPU_SETSIZE&&i<16;i++)if(CPU_ISSET(i,&mask))printf(" %d",i);
    printf("\n  GPU DCs: pin NCCL threads to specific NUMA-local CPUs.\n");
    /* WHY: EEVDF (Earliest Eligible Virtual Deadline First, Linux 6.6) replaced CFS.
     *      CFS used vruntime (accumulated CPU time) to pick the "most starved" task.
     *      Problem: CFS could not express latency requirements -- all normal tasks
     *      had equal latency.  EEVDF adds a virtual deadline: tasks get a time slice
     *      allocation, and the scheduler picks the task whose virtual deadline is earliest
     *      among those that are "eligible" (have accumulated the minimum runtime).
     *      latency_nice: a new per-task attribute (like nice but for latency) that adjusts
     *      how quickly a task gets scheduled after being woken up.  Used for interactive tasks.
     *
     * WHY: nice() changes CFS/EEVDF weight, not timeslice directly.
     *      nice(-20): weight=88761; nice(0): weight=1024; nice(+19): weight=15.
     *      A nice(-20) process gets 88761/(88761+1024) = 98.8% of CPU time when competing
     *      with a nice(0) process.  This makes nice() very powerful for prioritization.
     */
    ps("Phase 5: EEVDF Scheduler (Linux 6.6+) and nice() Impact");
    printf("  EEVDF replaced CFS in Linux 6.6:\n");
    printf("    CFS: pick task with smallest vruntime (most CPU-starved)\n");
    printf("    EEVDF: pick ELIGIBLE task with earliest virtual DEADLINE\n");
    printf("    Eligibility: task has accumulated >= its minimum runtime\n");
    printf("    Benefit: better latency for interactive tasks with latency_nice\n\n");

    /* Show current nice value and measure its impact */
    int current_nice = getpriority(PRIO_PROCESS, 0);
    printf("  Current nice value: %d\n", current_nice);

    /* Read sched tunables */
    {
        const char *tunables[] = {
            "/proc/sys/kernel/sched_min_granularity_ns",
            "/proc/sys/kernel/sched_latency_ns",
            "/proc/sys/kernel/sched_migration_cost_ns",
            NULL
        };
        for (int i = 0; tunables[i]; i++) {
            FILE *fp2 = fopen(tunables[i], "r");
            if (fp2) {
                char val[64] = {0};
                if (fgets(val, sizeof(val), fp2)) {
                    val[strcspn(val, "\n")] = '\0';
                    /* Just show basename */
                    const char *name = strrchr(tunables[i], '/') + 1;
                    printf("  %s: %s ns\n", name, val);
                }
                fclose(fp2);
            }
        }
    }

    /* OBSERVE: nice impact simulation */
    printf("\n  nice() weight table (selected values):\n");
    printf("    nice -20: weight=88761 (gets ~98.8%% vs nice 0)\n");
    printf("    nice  -5: weight=3121  (gets ~75%% vs nice 0)\n");
    printf("    nice   0: weight=1024  (baseline)\n");
    printf("    nice  +5: weight=335   (gets ~24%% vs nice 0)\n");
    printf("    nice +19: weight=15    (gets ~1.4%% vs nice 0)\n");

    /* === EXERCISE ===
     * Try these hands-on tasks:
     * 1. Run two CPU-bound loops simultaneously at different nice values:
     *      nice -n -10 ./cpu_burn &
     *      nice -n +10 ./cpu_burn &
     *    Monitor with top or htop -- the nice -10 process should get ~16x more CPU time.
     *    Use /proc/<PID>/sched to read se.vruntime of both processes.
     * 2. Measure scheduling latency with SCHED_FIFO vs SCHED_OTHER:
     *    Write a thread that measures wake-up latency (time from pthread_cond_signal
     *    to consumer thread starting after pthread_cond_wait).
     *    Compare SCHED_FIFO priority=10 vs SCHED_OTHER nice=0.
     *    SCHED_FIFO should have lower and more consistent latency.
     * 3. Modern (EEVDF in Linux 6.6): read /proc/self/sched after upgrading to Linux 6.6+.
     *    Look for 'se.deadline' field (not present in CFS/older kernels).
     *    Experiment: cat /proc/sys/kernel/sched_min_granularity_ns and compare with
     *    EEVDF's slice parameter in /proc/self/sched.
     *
     * OBSERVE: SCHED_FIFO with priority 99 can starve ALL other processes.
     *          On a single-CPU system, a SCHED_FIFO process that never yields will
     *          prevent the kernel from running any other task (including the watchdog).
     *          This is why SCHED_FIFO requires CAP_SYS_NICE or root.
     * WHY:     CPU affinity (sched_setaffinity) is essential for NUMA-aware GPU workloads.
     *          Pinning NCCL communication threads to CPUs on the same NUMA node as the GPU's
     *          PCIe slot reduces memory access latency by 2x (local vs remote DRAM).
     */
    printf("\n========== Hands-On Exercise ==========\n");
    printf("1. Run two CPU-bound loops at nice -10 and nice +10; verify ~16x CPU ratio.\n");
    printf("2. Measure wake-up latency for SCHED_FIFO vs SCHED_OTHER;\n");
    printf("   verify FIFO has lower and more consistent latency.\n");
    printf("3. Modern (EEVDF Linux 6.6): look for se.deadline in /proc/self/sched;\n");
    printf("   compare EEVDF slice with sched_min_granularity_ns tunable.\n");

    printf("\n========== Quiz ==========\n");
    printf("Q1. How does CFS ensure fairness using vruntime?\n");
    printf("Q2. Why would you use SCHED_FIFO for GPU communication threads?\n");
    printf("Q3. What is the risk of using SCHED_FIFO without care? (hint: starvation)\n");
    printf("Q4. How does CPU affinity interact with NUMA topology?\n");
    printf("Q5. What is EEVDF and how does it differ from CFS in the scheduling decision?\n");
    printf("    What is 'latency_nice' and which workloads benefit from it?\n");
    printf("Q6. How does the nice value weight formula work?  What CPU fraction does\n");
    printf("    a nice -20 process get when competing with a nice 0 process?\n");
    printf("Q7. What is SCHED_DEADLINE and what three parameters does it require?\n");
    printf("    Why is it suitable for real-time GPU kernel dispatch scheduling?\n");
    return 0;
}
