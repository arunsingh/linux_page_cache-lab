/*
 * lab_11_tlb_hugepages.c
 * Topic: TLB, Large Pages, Boot Sector
 * Build: gcc -O0 -Wall -o lab_11 lab_11_tlb_hugepages.c
 * Run:   ./lab_11   (or: perf stat -e dTLB-load-misses ./lab_11)
 *
 * Author: Arun Singh | arunsingh.in@gmail.com
 * OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
static void print_section(const char *t){printf("\n========== %s ==========\n",t);}
static double now_sec(void){struct timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);return ts.tv_sec+ts.tv_nsec/1e9;}

static void demo_tlb_pressure(void){
    print_section("Phase 1: TLB Pressure — 4K vs Sequential Stride");
    size_t sz=256*1024*1024;
    char *mem=mmap(NULL,sz,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(!mem||mem==MAP_FAILED){perror("mmap");return;}
    memset(mem,1,sz);
    long ps=sysconf(_SC_PAGESIZE);
    /* Stride=page_size -> touches every page -> TLB thrash */
    double t0=now_sec();
    volatile long sum=0;
    for(size_t i=0;i<sz;i+=(size_t)ps) sum+=mem[i];
    double t_page=now_sec()-t0;
    /* Stride=64 -> sequential within pages -> TLB friendly */
    t0=now_sec();sum=0;
    for(size_t i=0;i<sz;i+=64) sum+=mem[i];
    double t_seq=now_sec()-t0;
    printf("  Page-stride (%ld B): %.4fs\n",ps,t_page);
    printf("  Cache-line stride (64B): %.4fs\n",t_seq);
    printf("  Ratio: %.1fx\n",t_page/t_seq);
    printf("\n  OBSERVE: Page-stride is slower despite touching FEWER bytes because\n");
    printf("  it accesses more unique pages, causing more TLB misses.\n");
    printf("  TIP: Run with 'perf stat -e dTLB-load-misses' to see actual TLB miss counts.\n");
    munmap(mem,sz);
}

static void demo_hugepages(void){
    print_section("Phase 2: Huge Pages");
    FILE *fp=fopen("/proc/meminfo","r");
    if(!fp)return;
    char line[256];
    while(fgets(line,sizeof(line),fp)){
        if(strstr(line,"HugePages")||strstr(line,"Hugepagesize"))
            printf("  %s",line);
    }
    fclose(fp);
    /* Try to allocate a huge page */
    size_t hp_sz=2*1024*1024;
    void *hp=mmap(NULL,hp_sz,PROT_READ|PROT_WRITE,
                  MAP_PRIVATE|MAP_ANONYMOUS|MAP_HUGETLB,-1,0);
    if(hp!=MAP_FAILED){
        memset(hp,0xAB,hp_sz);
        printf("\n  Successfully allocated 2MiB huge page at %p\n",hp);
        printf("  One TLB entry covers 2MiB instead of 4KiB = 512x better coverage.\n");
        munmap(hp,hp_sz);
    } else {
        printf("\n  Huge page allocation failed (need: echo 10 > /proc/sys/vm/nr_hugepages)\n");
        printf("  In GPU DCs, huge pages are critical for DMA buffers and RDMA registrations.\n");
    }
}

static void demo_boot_overview(void){
    print_section("Phase 3: Boot Sequence Overview");
    printf("  x86 boot sequence:\n");
    printf("  1. BIOS/UEFI: POST, find boot device\n");
    printf("  2. Bootloader (GRUB): load kernel + initramfs into memory\n");
    printf("  3. Real mode -> Protected mode -> Long mode (64-bit)\n");
    printf("  4. Kernel: setup page tables, enable paging, start scheduler\n");
    printf("  5. init/systemd (PID 1): mount filesystems, start services\n\n");
    printf("  Boot timeline from dmesg:\n");
    FILE *fp=popen("dmesg 2>/dev/null | head -20","r");
    if(fp){char l[256];while(fgets(l,sizeof(l),fp))printf("  %s",l);pclose(fp);}
    else printf("  (need root for dmesg)\n");
}

/* WHY: TLB shootdown: when the kernel changes a page table mapping (remap, munmap,
 *      mprotect), it must invalidate the corresponding TLB entry on ALL CPUs that
 *      might have cached it.  On x86_64, this is done by sending an IPI (inter-
 *      processor interrupt) to each CPU, which runs INVLPG for the affected address.
 *      On systems with 256+ cores, TLB shootdowns are expensive: O(N_CPU) IPIs.
 *      This is why kernel code batches shootdowns (flush_tlb_range) and why
 *      mprotect() on a large range is slow on many-core machines.
 *
 * WHY: THP (Transparent Huge Pages) for GPU memory:  The NVIDIA CUDA driver uses
 *      THP for large host-pinned allocations to improve DMA mapping performance.
 *      When cudaMallocHost allocates a large buffer, if THP is enabled the kernel
 *      can map it with 2 MiB PTEs, reducing the number of IOMMU entries needed.
 *      Fewer IOMMU entries = faster DMA mapping setup = lower latency for PCIe transfers.
 */

static void demo_strided_benchmark(void){
    print_section("Phase 4: Sequential vs Strided Access Benchmark");
    size_t sz = 128 * 1024 * 1024; /* 128 MiB */
    char *mem = mmap(NULL, sz, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if (!mem || mem == MAP_FAILED) { perror("mmap"); return; }
    memset(mem, 1, sz);

    long ps = sysconf(_SC_PAGESIZE);
    volatile long sum = 0;

    /* Sequential: stride 1 byte -- best cache behavior */
    double t0 = now_sec();
    for (size_t i = 0; i < sz; i += 1) sum += mem[i];
    double t_seq1 = now_sec() - t0;

    /* Stride = cache line (64B): good but skips within cache line */
    t0 = now_sec(); sum = 0;
    for (size_t i = 0; i < sz; i += 64) sum += mem[i];
    double t_cl = now_sec() - t0;

    /* Stride = page size (4096B): poor TLB behavior, every access is new page */
    t0 = now_sec(); sum = 0;
    for (size_t i = 0; i < sz; i += (size_t)ps) sum += mem[i];
    double t_page = now_sec() - t0;

    /* Stride = 2 MiB: huge page boundary, maximizes TLB misses with 4K pages */
    t0 = now_sec(); sum = 0;
    for (size_t i = 0; i < sz; i += 2*1024*1024) sum += mem[i];
    double t_huge = now_sec() - t0;

    printf("  Stride 1B   (sequential):   %.4f s  (best cache + TLB behavior)\n", t_seq1);
    printf("  Stride 64B  (cache-line):   %.4f s\n", t_cl);
    printf("  Stride 4KB  (page-stride):  %.4f s  (TLB thrash with 4K pages)\n", t_page);
    printf("  Stride 2MB  (huge-stride):  %.4f s  (TLB thrash regardless of page size)\n", t_huge);
    printf("\n  Run: perf stat -e dTLB-load-misses,iTLB-load-misses ./lab_11\n");
    printf("       to measure actual TLB miss counts per access pattern.\n");

    /* OBSERVE: With THP enabled, stride-4K should be faster than without THP
     *          because 2 MiB THP entries cover 512 consecutive 4K accesses with 1 TLB entry. */
    printf("\n  OBSERVE: Enable THP (echo always > /sys/kernel/mm/transparent_hugepage/enabled)\n");
    printf("           and re-run: stride-4K should improve as THP covers 512 pages per entry.\n");

    munmap(mem, sz);
}

int main(void){
    /* EXPECTED OUTPUT (Linux x86_64):
     *   Phase 1: Page-stride ratio ~1.5-3x slower than cache-line stride
     *   Phase 2: HugePages_Total=0 if not pre-allocated; allocation fails with tip
     *   Phase 3: dmesg boot sequence (or "need root" message)
     *   Phase 4: Sequential 1B < CL 64B << page-stride << 2MB-stride (normalized by bytes)
     */
    printf("=== Lab 11: TLB, Large Pages, Boot Sector ===\n");
    demo_tlb_pressure();
    demo_hugepages();
    demo_boot_overview();
    demo_strided_benchmark();

    /* === EXERCISE ===
     * Try these hands-on tasks:
     * 1. Benchmark the 128 MiB array access with and without THP enabled:
     *      echo always > /sys/kernel/mm/transparent_hugepage/enabled  (then run)
     *      echo never  > /sys/kernel/mm/transparent_hugepage/enabled  (then run)
     *    Use perf stat -e dTLB-load-misses to count TLB misses in each case.
     *    Expect 512x fewer TLB misses with THP for the page-stride access pattern.
     * 2. Measure TLB shootdown cost: in a loop, call mprotect() to toggle a page's
     *    permissions between PROT_READ and PROT_READ|PROT_WRITE.  Each mprotect
     *    triggers a TLB shootdown IPI to all CPUs.  Use perf to count:
     *      perf stat -e tlb:tlb_flush ./lab_11
     * 3. Modern (THP for GPU memory): CUDA runtime uses madvise(MADV_HUGEPAGE) for
     *    pinned host buffers.  Profile a cudaMemcpy with and without THP:
     *      sudo sh -c 'echo always > /sys/kernel/mm/transparent_hugepage/enabled'
     *    The IOMMU mapping time for DMA buffers drops with fewer IOMMU entries.
     *    Check: cat /sys/kernel/iommu_groups/*/type to see IOMMU group assignments.
     *
     * OBSERVE: TLB shootdowns are the hidden cost of mprotect() on many-core servers.
     *          On a 128-core system, one mprotect() can trigger 127 IPIs.
     *          This is why mlock() + never changing permissions is faster than
     *          repeatedly mprotect-ing buffers in a DPDK or SPDK application.
     * WHY:     Each CPU has its own TLB.  When a page table entry is modified by CPU-0,
     *          CPUs 1-N may still have the old translation cached.  The INVLPG IPI
     *          forces each CPU to flush that specific entry from its TLB.
     */
    printf("\n========== Hands-On Exercise ==========\n");
    printf("1. Benchmark page-stride access with THP always vs never;\n");
    printf("   use perf to count dTLB-load-misses and observe 512x improvement.\n");
    printf("2. Measure TLB shootdown cost: loop calling mprotect() and count\n");
    printf("   tlb:tlb_flush events with perf stat.\n");
    printf("3. Modern (THP for GPU/CUDA): enable THP and measure IOMMU mapping\n");
    printf("   time for large DMA buffers; check /sys/kernel/iommu_groups/.\n");

    printf("\n========== Quiz ==========\n");
    printf("Q1. Why does page-stride access cause more TLB misses than cache-line stride?\n");
    printf("Q2. How do 2MiB huge pages reduce TLB pressure? Calculate the coverage ratio.\n");
    printf("Q3. When the kernel boots, paging is not yet enabled. How does it access memory?\n");
    printf("Q4. Why are huge pages important for GPU DMA and RDMA in data centres?\n");
    printf("Q5. What is the role of initramfs in the boot process?\n");
    printf("Q6. What is a TLB shootdown and why is it expensive on many-core (128+ CPU) servers?\n");
    printf("    How does the kernel use INVLPG IPIs to maintain TLB coherence?\n");
    printf("Q7. How does enabling THP (Transparent Huge Pages) reduce TLB miss count for\n");
    printf("    an ML workload loading large model weight tensors into memory?\n");
    return 0;
}
