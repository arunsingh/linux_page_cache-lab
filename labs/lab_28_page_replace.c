/*
 * lab_28_page_replace.c
 * Topic: Page Replacement, Thrashing
 * Build: gcc -O0 -Wall -pthread -o lab_28 lab_28_page_replace.c
 *
 * Author: Arun Singh | arunsingh.in@gmail.com
 * OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
*/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <time.h>
static void ps(const char *t){printf("\n========== %s ==========\n",t);}
static double now(void){struct timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);return ts.tv_sec+ts.tv_nsec/1e9;}
int main(void){
    printf("=== Lab 28: Page Replacement, Thrashing ===\n");
    ps("Phase 1: Working Set Concept");
    printf("  Working set = set of pages a process actively uses.\n");
    printf("  If working set fits in RAM -> fast. If not -> thrashing.\n\n");
    /* Sequential vs random access pattern */
    size_t sz=64*1024*1024;
    char *mem=mmap(NULL,sz,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    memset(mem,1,sz);
    /* Sequential = good locality */
    double t0=now();long s=0;
    for(int r=0;r<3;r++)for(size_t i=0;i<sz;i+=4096)s+=mem[i];
    printf("  Sequential scan (64MiB, 3x): %.3fs\n",now()-t0);
    /* Random = poor locality */
    srand(42);t0=now();
    for(int i=0;i<(int)(sz/4096)*3;i++)s+=mem[(rand()%(sz/4096))*4096];
    printf("  Random access (same count):   %.3fs\n",now()-t0);
    printf("  OBSERVE: Random access is slower due to poor spatial/temporal locality.\n");
    munmap(mem,sz);
    ps("Phase 2: Linux Page Replacement");
    printf("  Linux uses a modified LRU with active/inactive lists:\n");
    printf("  - Active list:   recently accessed pages (hot)\n");
    printf("  - Inactive list: candidates for eviction (cold)\n");
    printf("  - Pages promote active->inactive->eviction\n");
    printf("  - Accessed bit from PTE drives promotion/demotion\n\n");
    FILE *fp=fopen("/proc/meminfo","r");
    if(fp){char l[256];
        while(fgets(l,sizeof(l),fp)){
            if(strstr(l,"Active")||strstr(l,"Inactive")||strstr(l,"Unevictable"))
                printf("  %s",l);}
        fclose(fp);}
    /* OBSERVE: /proc/vmstat pgmajfault counter increments on every major fault.
     *          A rate > 100/s on a server typically indicates active swap pressure.
     *          kswapd CPU usage rising to >5% is another thrashing indicator. */
    ps("Phase 3: Thrashing Detection");
    printf("  Thrashing = system spends more time paging than doing useful work.\n");
    printf("  Symptoms: high pgmajfault in /proc/vmstat, load average spikes.\n");
    printf("  Solutions: add RAM, reduce working set, use cgroup memory limits.\n");
    printf("  GPU DCs: thrashing in host memory delays GPU data pipeline.\n");

    /* WHY: MGLRU (Multi-Generation LRU), merged in Linux 6.1, replaces the classic
     *      two-list (active/inactive) approach with a generational model.  Pages are
     *      grouped into "generations" based on age.  The oldest generation is evicted
     *      first.  This dramatically reduces false evictions of hot pages during scans.
     *      Android adopted MGLRU; it is now the default in most distro kernels >= 6.1. */
    ps("Phase 4: Modern Linux Page Replacement (MGLRU)");
    printf("  Classic LRU (< Linux 6.1): two lists -- active (hot) and inactive (cold).\n");
    printf("  Problem: large sequential reads thrash the active list, evicting truly hot pages.\n\n");
    printf("  MGLRU (Linux 6.1+): generational LRU with 4 generations per memory cgroup.\n");
    printf("    - gen[0] = newest (most recently accessed)\n");
    printf("    - gen[3] = oldest (eviction candidates)\n");
    printf("    - Pages age through generations via the Accessed PTE bit sweep.\n");
    printf("    - Read-ahead pages start in gen[1] (not gen[0]) to avoid polluting hot data.\n\n");

    /* Check if MGLRU is available on this kernel */
    FILE *mglru = fopen("/sys/kernel/mm/lru_gen/enabled", "r");
    if (mglru) {
        char val[16] = {0};
        if (fgets(val, sizeof(val), mglru)) {
            val[strcspn(val, "\n")] = '\0';
            printf("  MGLRU enabled (/sys/kernel/mm/lru_gen/enabled): %s\n", val);
        }
        fclose(mglru);
    } else {
        printf("  /sys/kernel/mm/lru_gen/enabled not found (kernel < 6.1 or not compiled in).\n");
    }

    /* Show swap usage */
    {
        FILE *fp = fopen("/proc/meminfo", "r");
        if (fp) {
            char l[256];
            printf("\n  Swap usage from /proc/meminfo:\n");
            while (fgets(l, sizeof(l), fp)) {
                if (strncmp(l,"SwapTotal",9)==0 || strncmp(l,"SwapFree",8)==0 ||
                    strncmp(l,"SwapCached",10)==0)
                    printf("    %s", l);
            }
            fclose(fp);
        }
    }

    /* Show major/minor fault counters */
    {
        FILE *fp = fopen("/proc/vmstat", "r");
        if (fp) {
            char l[256];
            printf("\n  Page fault counters from /proc/vmstat:\n");
            while (fgets(l, sizeof(l), fp)) {
                if (strncmp(l,"pgfault",7)==0 || strncmp(l,"pgmajfault",10)==0 ||
                    strncmp(l,"pgpgout",7)==0 || strncmp(l,"pswpout",7)==0)
                    printf("    %s", l);
            }
            fclose(fp);
        }
    }

    /* === EXERCISE ===
     * Try these hands-on tasks:
     * 1. Monitor swap pressure: run this binary while watching swap in real time:
     *      watch -n1 'grep -E "Swap|pgmajfault" /proc/meminfo /proc/vmstat'
     *    Increase sz to 2x your available RAM and observe swap activity.
     *    Note: on systems with no swap configured, the OOM killer fires instead.
     * 2. Compare MGLRU vs classic LRU behaviour: if kernel >= 6.1, toggle MGLRU:
     *      echo 0 > /sys/kernel/mm/lru_gen/enabled   (disable MGLRU)
     *    Run the sequential vs random access benchmark and compare times.
     *    Re-enable: echo 1 > /sys/kernel/mm/lru_gen/enabled
     * 3. Modern (GPU datacenter): large language model training uses pinned host
     *    memory as a staging buffer for GPU data.  Allocate with mlock():
     *      mlock(mem, sz);  // prevent this region from being swapped
     *    Check /proc/self/status VmLck field.  Observe that mlock'd pages never
     *    appear in swap even under memory pressure -- critical for NCCL collectives.
     *
     * OBSERVE: Sequential access causes far fewer TLB misses and page faults than
     *          random access to the same total data, because spatial locality keeps
     *          the working set small and the prefetcher warm.
     * WHY:     MGLRU uses the PTE Accessed bit sweep (similar to a software clock
     *          algorithm) to age pages without scanning all page frames.  It amortizes
     *          the cost of the sweep across kswapd wakeup periods.
     */
    printf("\n========== Hands-On Exercise ==========\n");
    printf("1. Allocate more than available RAM; watch swap grow in /proc/meminfo;\n");
    printf("   monitor pgmajfault rate in /proc/vmstat to detect thrashing onset.\n");
    printf("2. If kernel >= 6.1: toggle MGLRU via /sys/kernel/mm/lru_gen/enabled\n");
    printf("   and compare sequential vs random access benchmark performance.\n");
    printf("3. Modern (GPU/ML): use mlock() to pin host staging buffers;\n");
    printf("   verify VmLck in /proc/self/status shows pinned memory size.\n");

    printf("\n========== Quiz ==========\n");
    printf("Q1. Explain the working set model and why it matters for performance.\n");
    printf("Q2. How does Linux approximate LRU without scanning all pages?\n");
    printf("Q3. What is the difference between page-out and swap-out?\n");
    printf("Q4. How would you detect thrashing on a production server?\n");
    printf("Q5. What problem does MGLRU solve compared to the classic two-list LRU,\n");
    printf("    and when was it merged into the mainline Linux kernel?\n");
    printf("Q6. Why must GPU/ML workloads use mlock() for DMA staging buffers,\n");
    printf("    and what happens if those pages are swapped out mid-transfer?\n");
    printf("Q7. Explain the clock algorithm and how it approximates LRU using\n");
    printf("    the PTE Accessed bit.  How does kswapd amortize its cost in Linux 6.x?\n");
    return 0;
}
