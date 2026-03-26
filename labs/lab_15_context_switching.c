/*
 * lab_15.c
 * Topic: Process structure, Context switching
 * Build: gcc -O0 -Wall -pthread -o lab_15 lab_15_*.c
 *
 * Author: Arun Singh | arunsingh.in@gmail.com
 * OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sched.h>
#include <pthread.h>
static void print_section(const char *t){printf("\n========== %s ==========\n",t);}
static double now_ns(void){struct timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);return ts.tv_sec*1e9+ts.tv_nsec;}
static void demo_ctx_switch_cost(void){
    print_section("Phase 1: Context Switch Cost Measurement");
    int p1[2],p2[2];
    pipe(p1);pipe(p2);
    char c=0;int N=100000;
    pid_t pid=fork();
    if(pid==0){
        for(int i=0;i<N;i++){read(p1[0],&c,1);write(p2[1],&c,1);}
        _exit(0);
    }
    double t0=now_ns();
    for(int i=0;i<N;i++){write(p1[1],&c,1);read(p2[0],&c,1);}
    double elapsed=now_ns()-t0;
    wait(NULL);
    printf("  %d round-trips via pipe: %.0f ns total\n",N,elapsed);
    printf("  Per context switch: ~%.0f ns (%.1f us)\n",elapsed/(2.0*N),elapsed/(2000.0*N));
    printf("  Modern server: typically 1-5 us per context switch.\n");
}
static void *thread_ping(void *arg){
    int *fds=(int*)arg;char c=0;
    for(int i=0;i<100000;i++){read(fds[0],&c,1);write(fds[1],&c,1);}
    return NULL;
}
static void demo_thread_switch(void){
    print_section("Phase 2: Thread vs Process Context Switch");
    int p1[2],p2[2];pipe(p1);pipe(p2);
    int fds[2]={p1[0],p2[1]};
    pthread_t t;char c=0;int N=100000;
    pthread_create(&t,NULL,thread_ping,fds);
    double t0=now_ns();
    for(int i=0;i<N;i++){write(p1[1],&c,1);read(p2[0],&c,1);}
    double elapsed=now_ns()-t0;
    pthread_join(t,NULL);
    printf("  Thread switch: ~%.0f ns per switch\n",elapsed/(2.0*N));
    printf("  OBSERVE: Thread switches are often faster — no TLB flush, shared mm.\n");
}
static void demo_voluntary_stats(void){
    print_section("Phase 3: Context Switch Stats");
    char path[128];
    snprintf(path,sizeof(path),"/proc/%d/status",getpid());
    FILE *fp=fopen(path,"r");
    if(!fp)return;
    char line[256];
    while(fgets(line,sizeof(line),fp)){
        if(strstr(line,"ctxt_switches")||strstr(line,"voluntary"))printf("  %s",line);
    }
    fclose(fp);
    printf("  voluntary = process yielded CPU (I/O, sleep, mutex)\n");
    printf("  nonvoluntary = preempted by scheduler (time slice expired)\n");
}
/* WHY: getpid() is implemented via vdso on modern Linux (5.x+).  The kernel
 *      caches the PID in the vdso data page so the C library can return it without
 *      issuing a syscall at all.  getpid() takes ~1-5 ns; a real syscall takes ~100 ns.
 *      This is why high-frequency code (e.g., logging libraries) calls getpid() freely.
 *
 * WHY: On x86_64, a full syscall involves: SYSCALL instruction, kernel entry trampoline,
 *      CR3 switch (if KPTI enabled, for Meltdown mitigation), register save, handler,
 *      register restore, CR3 switch back, SYSRET.  Each CR3 switch flushes TLB entries
 *      not tagged with the PCID, adding ~100-200 cycles on top of the syscall itself.
 */

static void demo_getpid_latency(void) {
    print_section("Phase 4: getpid() Syscall Latency (vdso vs raw)");
    int N = 1000000;

    /* Measure getpid() — likely served from vdso, no kernel crossing */
    double t0 = now_ns();
    volatile pid_t dummy = 0;
    for (int i = 0; i < N; i++) dummy = getpid();
    double elapsed_vdso = now_ns() - t0;
    printf("  getpid() x%d: %.0f ns total, ~%.2f ns/call\n",
           N, elapsed_vdso, elapsed_vdso / N);
    printf("  (If ~1-5 ns/call: served by vdso without kernel crossing)\n");
    printf("  (If ~100-300 ns/call: full syscall path, KPTI TLB cost visible)\n");

    /* OBSERVE: On kernels with KPTI (most x86 post-2018), even vdso-bypassed calls
     *          that do reach the kernel pay a CR3 switch penalty (~100-200 cycles). */
    printf("\n  Check if KPTI is active: grep 'cpu_bugs' /proc/cpuinfo | grep meltdown\n");
    printf("  With KPTI: each real syscall costs ~200 ns extra vs non-KPTI kernel.\n");
    (void)dummy;
}

int main(void){
    /* EXPECTED OUTPUT (Linux x86_64):
     *   === Lab 15: Process Structure, Context Switching ===
     *   Phase 1: 100000 pipe round-trips, per ctx switch ~2-10 us (process)
     *   Phase 2: thread switch typically 30-60% faster than process switch
     *   Phase 3: voluntary_ctxt_switches: small number (we did I/O wait via pipe)
     *            nonvoluntary_ctxt_switches: near 0 (short-lived, no preemption)
     *   Phase 4: getpid() ~1-5 ns/call if vdso active, ~100-300 ns if syscall path
     */
    printf("=== Lab 15: Process Structure, Context Switching ===\n");
    demo_ctx_switch_cost();
    demo_thread_switch();
    demo_voluntary_stats();
    demo_getpid_latency();

    /* === EXERCISE ===
     * Try these hands-on tasks:
     * 1. Run this binary with 'perf stat -e context-switches ./lab_15' (Linux with
     *    perf installed).  Compare the hw counter to the pipe-based estimate.
     *    Then repeat inside a Docker container: does perf still work?
     *    (Hint: perf in containers needs --cap-add SYS_ADMIN or seccomp adjustment.)
     * 2. Measure getpid() latency on a KPTI kernel vs a patched/mitigated one:
     *    cat /sys/devices/system/cpu/vulnerabilities/meltdown
     *    If it says "Mitigation: PTI" you're paying the CR3 switch cost on every syscall.
     * 3. Modern (VDSO eliminates syscall overhead): write a tight loop calling
     *    clock_gettime(CLOCK_REALTIME) and clock_gettime(CLOCK_TAI) — both go via vdso.
     *    Then strace ./lab_15 2>&1 | grep clock_gettime and observe: no syscall entries!
     *    This is the vdso optimization that makes high-frequency timestamping free.
     *
     * OBSERVE: Thread context switches are faster because they share the same mm_struct
     *          (page tables).  No CR3 register change = no TLB flush = faster resume.
     *          Process switches must reload CR3, invalidating TLB entries (unless PCID).
     * WHY:     Modern CPUs use PCID (Process Context ID) to tag TLB entries per-process,
     *          avoiding full TLB flushes on context switch.  Linux has supported PCID
     *          since 4.14.  With PCID, process switch cost drops significantly.
     */
    printf("\n========== Hands-On Exercise ==========\n");
    printf("1. Use 'perf stat -e context-switches ./lab_15' to count hardware context switches.\n");
    printf("2. Check /sys/devices/system/cpu/vulnerabilities/meltdown for KPTI status;\n");
    printf("   measure getpid() latency and correlate with PTI mitigation overhead.\n");
    printf("3. Modern (vdso): strace this binary and observe clock_gettime() does NOT\n");
    printf("   appear in strace output -- it is served entirely from vdso.\n");

    printf("\n========== Quiz ==========\n");
    printf("Q1. What state must the kernel save/restore during a context switch?\n");
    printf("Q2. Why are thread context switches cheaper than process switches?\n");
    printf("Q3. What is a voluntary vs nonvoluntary context switch?\n");
    printf("Q4. How does context switch cost affect GPU DC performance (hint: NCCL, latency)?\n");
    printf("Q5. What is the vdso and which syscalls does it accelerate on Linux x86_64?\n");
    printf("Q6. How does PCID (Process Context ID) reduce the TLB flush cost of\n");
    printf("    context switches, and which Linux version introduced PCID support?\n");
    printf("Q7. With KPTI enabled (Meltdown mitigation), why does every syscall cost\n");
    printf("    an extra ~100-200 cycles, and how does Linux mitigate this overhead\n");
    printf("    (hint: PCID + flush-on-return optimization)?\n");
    return 0;
}
