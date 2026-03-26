/*
 * lab_04_phys_mem_io.c
 *
 * Topic: Physical Memory Map, I/O, Segmentation
 *
 * Build: gcc -O0 -Wall -o lab_04 lab_04_phys_mem_io.c
 *
 * Author: Arun Singh | arunsingh.in@gmail.com
 * OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

static void print_section(const char *t) { printf("\n========== %s ==========\n", t); }

static void demo_iomem(void) {
    print_section("Phase 1: Physical Memory Map (/proc/iomem)");
    printf("  /proc/iomem shows how physical address space is divided:\n");
    printf("  RAM, reserved BIOS regions, PCI MMIO bars, GPU framebuffer, etc.\n\n");

    FILE *fp = fopen("/proc/iomem", "r");
    if (!fp) { printf("  (need root to read /proc/iomem — run: sudo cat /proc/iomem)\n"); return; }
    char line[256]; int n = 0;
    while (fgets(line, sizeof(line), fp) && n++ < 30) printf("  %s", line);
    fclose(fp);
    printf("\n  OBSERVE: 'System RAM' = usable memory. PCI regions = device MMIO.\n");
    printf("  In GPU servers, you'll see large MMIO regions for each GPU's BAR space.\n");
}

static void demo_ioports(void) {
    print_section("Phase 2: I/O Ports (/proc/ioports)");
    FILE *fp = fopen("/proc/ioports", "r");
    if (!fp) { printf("  (need root — run: sudo cat /proc/ioports)\n"); return; }
    char line[256]; int n = 0;
    while (fgets(line, sizeof(line), fp) && n++ < 15) printf("  %s", line);
    fclose(fp);
    printf("\n  OBSERVE: Legacy x86 uses port I/O (in/out instructions).\n");
    printf("  Modern devices use MMIO instead — memory-mapped into physical address space.\n");
}

static void demo_segments(void) {
    print_section("Phase 3: Segmentation (historical context)");
    printf("  x86 segmentation: code segment (CS), data segment (DS), stack segment (SS).\n");
    printf("  In 64-bit Linux, segmentation is essentially disabled:\n");
    printf("    CS, DS, SS all have base=0, limit=max (flat model).\n");
    printf("    FS/GS are used for thread-local storage (TLS) and percpu data.\n\n");

#if defined(__x86_64__)
    unsigned short cs, ds, ss, fs, gs;
    __asm__ volatile("mov %%cs, %0" : "=r"(cs));
    __asm__ volatile("mov %%ds, %0" : "=r"(ds));
    __asm__ volatile("mov %%ss, %0" : "=r"(ss));
    __asm__ volatile("mov %%fs, %0" : "=r"(fs));
    __asm__ volatile("mov %%gs, %0" : "=r"(gs));
    printf("  CS=0x%04x DS=0x%04x SS=0x%04x FS=0x%04x GS=0x%04x\n", cs, ds, ss, fs, gs);
    printf("  FS is non-zero = used for glibc TLS (thread-local storage).\n");
#endif
}

/* WHY: NUMA (Non-Uniform Memory Access) means that on multi-socket servers,
 *      memory accesses to remote NUMA nodes cost ~2x more latency than local.
 *      EPYC 9654 (Genoa) has 4 NUMA nodes per socket.  numactl and libnuma
 *      let processes bind to a NUMA node.  The kernel's NUMA balancing daemon
 *      migrates pages toward the CPU that accesses them most frequently.
 *
 * WHY: HBM (High Bandwidth Memory) on GPU cards (HBM2e on A100: 2 TB/s; HBM3 on H100)
 *      is directly attached to the GPU die via silicon interposer.  The host OS treats
 *      it as a separate NUMA node (via CXL or proprietary interface).  nvidia-smi can
 *      report the HBM occupancy.  cudaMalloc allocates from HBM; pinned host memory
 *      (cudaMallocHost) is allocated from DDR and registered for DMA.
 */

static void demo_meminfo_zones(void) {
    print_section("Phase 4: Memory Zones and NUMA (/proc/meminfo + /proc/buddyinfo)");
    printf("  Linux divides physical RAM into zones:\n");
    printf("    ZONE_DMA     (<16 MiB): legacy ISA DMA devices\n");
    printf("    ZONE_DMA32   (<4 GiB): 32-bit DMA devices (NVMe, older NICs)\n");
    printf("    ZONE_NORMAL  (>4 GiB): main kernel + user memory\n");
    printf("    ZONE_HIGHMEM (32-bit only): >896 MiB on 32-bit kernels (obsolete)\n\n");

    /* WHY: /proc/buddyinfo shows free memory blocks per order (2^order pages)
     *      in the buddy allocator.  Each column is the count of free 2^N-page blocks.
     *      Order 0 = 4K, order 1 = 8K, ... order 10 = 4MB.
     *      When order-10 blocks are scarce, large contiguous allocations fail -> OOM. */
    FILE *fp = fopen("/proc/buddyinfo", "r");
    if (fp) {
        char line[256];
        printf("  /proc/buddyinfo (free buddy blocks per order per NUMA node+zone):\n");
        while (fgets(line, sizeof(line), fp)) printf("  %s", line);
        fclose(fp);
    }

    /* Show key meminfo fields relevant to NUMA and zones */
    fp = fopen("/proc/meminfo", "r");
    if (fp) {
        char line[256];
        printf("\n  Selected /proc/meminfo fields:\n");
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line,"MemTotal",8)==0 || strncmp(line,"MemAvailable",12)==0 ||
                strncmp(line,"HugePages",9)==0 || strncmp(line,"Huge",4)==0 ||
                strncmp(line,"DirectMap",9)==0 || strncmp(line,"NFS",3)==0 ||
                strncmp(line,"Committed",9)==0)
                printf("    %s", line);
        }
        fclose(fp);
    }

    /* OBSERVE: DirectMap fields show how many pages are mapped by huge pages
     *          in the kernel's own linear mapping.  DirectMap2M > DirectMap4k
     *          means the kernel is efficiently using 2 MiB pages for its mappings. */
    printf("\n  OBSERVE: DirectMap2M and DirectMap1G show how much kernel memory\n");
    printf("           uses huge pages for its own linear map (better TLB coverage).\n");
    printf("  HBM in GPU systems: appears as a separate NUMA node in /proc/buddyinfo.\n");
}

int main(void) {
    /* EXPECTED OUTPUT (Linux x86_64):
     *   Phase 1: /proc/iomem with System RAM + PCI MMIO regions
     *   Phase 2: /proc/ioports with legacy I/O port ranges (0000-ffff)
     *   Phase 3: CS=0x0033 DS/SS=0x002b FS=non-zero (TLS base)
     *   Phase 4: /proc/buddyinfo per-zone free blocks; /proc/meminfo excerpts
     */
    printf("=== Lab 04: Physical Memory Map, I/O, Segmentation ===\n");
    demo_iomem();
    demo_ioports();
    demo_segments();
    demo_meminfo_zones();

    /* === EXERCISE ===
     * Try these hands-on tasks:
     * 1. Parse /proc/iomem and compute total System RAM by summing all 'System RAM'
     *    ranges.  Compare to MemTotal in /proc/meminfo.  Any discrepancy?
     *    (Hint: some RAM may be reserved by BIOS as 'Reserved' or 'ACPI Tables'.)
     * 2. Parse /proc/buddyinfo and compute total free memory in each zone by
     *    summing: count[order] * (2^order) * page_size for each order 0..10.
     *    Compare to MemFree in /proc/meminfo.  Are they equal?
     * 3. Modern (NUMA + HBM in GPU systems): on a multi-socket server run:
     *      numactl --hardware
     *      numastat -m
     *    to see NUMA node distances and per-node memory usage.  On a system with
     *    Intel Optane PMem or NVIDIA GPUs via CXL, additional NUMA nodes appear
     *    representing HBM/PMem -- the kernel manages them like regular memory nodes.
     *
     * OBSERVE: ZONE_DMA32 is critical for 32-bit DMA devices.  If this zone is
     *          exhausted, DMA allocations fail silently for older drivers that do
     *          not handle GFP_DMA32 failure correctly.
     * WHY:     The buddy allocator maintains separate free lists per zone so that
     *          ZONE_DMA32 allocations never steal from ZONE_NORMAL, ensuring DMA
     *          devices always have accessible memory below 4 GiB physical.
     */
    printf("\n========== Hands-On Exercise ==========\n");
    printf("1. Parse /proc/iomem: sum System RAM ranges; compare to /proc/meminfo MemTotal.\n");
    printf("2. Parse /proc/buddyinfo: compute free memory per zone from order counts.\n");
    printf("3. Modern (NUMA + HBM): run 'numactl --hardware' and 'numastat -m' on a\n");
    printf("   multi-socket server; identify HBM/CXL nodes if present.\n");

    printf("\n========== Quiz ==========\n");
    printf("Q1. What is the difference between port I/O and memory-mapped I/O?\n");
    printf("Q2. Why do GPU servers show large MMIO regions in /proc/iomem?\n");
    printf("Q3. Why is x86 segmentation mostly irrelevant in 64-bit Linux?\n");
    printf("Q4. What is the FS register used for in modern Linux userspace and kernel?\n");
    printf("Q5. What are the Linux memory zones (DMA, DMA32, NORMAL) and why\n");
    printf("    does the kernel need them?\n");
    printf("Q6. How does /proc/buddyinfo reveal memory fragmentation, and why does\n");
    printf("    high fragmentation cause huge page allocation failures?\n");
    printf("Q7. What is HBM (High Bandwidth Memory) in GPU systems, and how does the\n");
    printf("    OS expose it to applications -- as a NUMA node, a device, or something else?\n");
    return 0;
}
