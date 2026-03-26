/*
 * lab_02_pc_arch.c
 *
 * Topic: PC Architecture
 *
 * Demonstrates:
 *   1. CPUID instruction to query processor features
 *   2. Cache hierarchy discovery via sysfs
 *   3. NUMA topology exploration
 *   4. CPU feature flags relevant to modern servers/GPU DCs
 *
 * Build: gcc -O0 -Wall -pthread -o lab_02 lab_02_pc_arch.c
 * Run:   ./lab_02
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
#include <dirent.h>

static void print_section(const char *title) {
    printf("\n========== %s ==========\n", title);
}

static int read_sysfs_str(const char *path, char *buf, size_t len) {
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;
    if (!fgets(buf, (int)len, fp)) { fclose(fp); return -1; }
    buf[strcspn(buf, "\n")] = 0;
    fclose(fp);
    return 0;
}

static long read_sysfs_long(const char *path) {
    char buf[64];
    if (read_sysfs_str(path, buf, sizeof(buf)) != 0) return -1;
    return atol(buf);
}

/* --- Phase 1: CPUID --- */
#if defined(__x86_64__) || defined(__i386__)
static void cpuid(unsigned leaf, unsigned *eax, unsigned *ebx, unsigned *ecx, unsigned *edx) {
    __asm__ volatile("cpuid"
        : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
        : "a"(leaf), "c"(0));
}
#endif

static void demo_cpuid(void) {
    print_section("Phase 1: CPUID — Processor Identity");
#if defined(__x86_64__) || defined(__i386__)
    unsigned eax, ebx, ecx, edx;

    /* Vendor string */
    cpuid(0, &eax, &ebx, &ecx, &edx);
    char vendor[13];
    memcpy(vendor, &ebx, 4);
    memcpy(vendor + 4, &edx, 4);
    memcpy(vendor + 8, &ecx, 4);
    vendor[12] = 0;
    printf("  CPU Vendor: %s\n", vendor);
    printf("  Max CPUID leaf: %u\n", eax);

    /* Brand string */
    cpuid(0x80000000, &eax, &ebx, &ecx, &edx);
    if (eax >= 0x80000004) {
        char brand[49] = {0};
        for (unsigned i = 0; i < 3; i++) {
            cpuid(0x80000002 + i, &eax, &ebx, &ecx, &edx);
            memcpy(brand + i * 16, &eax, 4);
            memcpy(brand + i * 16 + 4, &ebx, 4);
            memcpy(brand + i * 16 + 8, &ecx, 4);
            memcpy(brand + i * 16 + 12, &edx, 4);
        }
        printf("  CPU Brand: %s\n", brand);
    }

    /* Feature flags */
    cpuid(1, &eax, &ebx, &ecx, &edx);
    printf("  Hyper-Threading: %s\n", (edx & (1 << 28)) ? "YES" : "NO");
    printf("  SSE2: %s\n", (edx & (1 << 26)) ? "YES" : "NO");
    printf("  AVX: %s\n", (ecx & (1 << 28)) ? "YES" : "NO");

    /* Extended features */
    cpuid(7, &eax, &ebx, &ecx, &edx);
    printf("  AVX2: %s\n", (ebx & (1 << 5)) ? "YES" : "NO");
    printf("  AVX-512F: %s\n", (ebx & (1 << 16)) ? "YES" : "NO");

    /* Hypervisor detection */
    cpuid(1, &eax, &ebx, &ecx, &edx);
    printf("  Running in VM: %s\n", (ecx & (1 << 31)) ? "YES (hypervisor bit set)" : "NO (bare metal likely)");
#else
    printf("  CPUID is x86-specific. Skipping on this architecture.\n");
    printf("  Check /proc/cpuinfo for equivalent information.\n");
#endif
}

/* --- Phase 2: Cache Hierarchy --- */

static void demo_cache_hierarchy(void) {
    print_section("Phase 2: Cache Hierarchy (sysfs)");

    printf("  Modern CPUs have L1 (split I/D), L2 (unified), L3 (shared).\n");
    printf("  GPU DC servers: cache coherency affects NCCL all-reduce performance.\n\n");

    char path[256], buf[128];
    for (int i = 0; i < 8; i++) {
        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu0/cache/index%d/type", i);
        if (read_sysfs_str(path, buf, sizeof(buf)) != 0) break;
        char type[32]; strncpy(type, buf, sizeof(type));

        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu0/cache/index%d/level", i);
        long level = read_sysfs_long(path);

        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu0/cache/index%d/size", i);
        read_sysfs_str(path, buf, sizeof(buf));

        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu0/cache/index%d/ways_of_associativity", i);
        long ways = read_sysfs_long(path);

        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu0/cache/index%d/coherency_line_size", i);
        long line_size = read_sysfs_long(path);

        printf("  L%ld %-12s size=%-8s ways=%-3ld line_size=%ld bytes\n",
               level, type, buf, ways, line_size);
    }

    printf("\nOBSERVE: L1 is split into Instruction and Data caches.\n");
    printf("         L3 is typically shared across cores on the same socket.\n");
    printf("         Cache line size (usually 64B) determines minimum transfer unit.\n");
}

/* --- Phase 3: CPU Topology --- */

static void demo_topology(void) {
    print_section("Phase 3: CPU Topology");

    long online = sysconf(_SC_NPROCESSORS_ONLN);
    long configured = sysconf(_SC_NPROCESSORS_CONF);
    printf("  CPUs configured: %ld\n", configured);
    printf("  CPUs online:     %ld\n", online);

    /* Check for NUMA */
    char buf[128];
    int numa_nodes = 0;
    for (int n = 0; n < 16; n++) {
        char path[256];
        snprintf(path, sizeof(path), "/sys/devices/system/node/node%d/cpulist", n);
        if (read_sysfs_str(path, buf, sizeof(buf)) == 0) {
            printf("  NUMA node %d: CPUs %s\n", n, buf);
            numa_nodes++;
        }
    }
    if (numa_nodes == 0) {
        printf("  NUMA: not detected (single-node or info unavailable)\n");
    }

    /* Show sibling info for hyperthreading */
    char path[256];
    snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu0/topology/thread_siblings_list");
    if (read_sysfs_str(path, buf, sizeof(buf)) == 0) {
        printf("  CPU0 HT siblings: %s\n", buf);
    }
    snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu0/topology/core_siblings_list");
    if (read_sysfs_str(path, buf, sizeof(buf)) == 0) {
        printf("  CPU0 socket siblings: %s\n", buf);
    }

    printf("\nOBSERVE: In GPU DCs, NUMA topology determines which GPUs are 'local' to which CPUs.\n");
    printf("         Incorrect CPU-GPU affinity can cause PCIe traffic to cross NUMA boundaries,\n");
    printf("         adding latency to NCCL operations.\n");
}

/* --- Phase 4: Memory hierarchy sizes --- */

static void demo_memory_info(void) {
    print_section("Phase 4: System Memory");

    FILE *fp = fopen("/proc/meminfo", "r");
    if (!fp) { perror("fopen"); return; }
    char line[256];
    const char *keys[] = {"MemTotal", "MemFree", "MemAvailable", "Cached", "SwapTotal",
                          "HugePages_Total", "Hugepagesize", NULL};
    while (fgets(line, sizeof(line), fp)) {
        for (int i = 0; keys[i]; i++) {
            if (strncmp(line, keys[i], strlen(keys[i])) == 0) {
                printf("  %s", line);
                break;
            }
        }
    }
    fclose(fp);

    printf("\nOBSERVE: GPU DC servers typically have 256GB-2TB RAM.\n");
    printf("         HugePages reduce TLB pressure for large memory workloads.\n");
}

/* WHY: /proc/iomem shows the physical address space layout assigned by BIOS/UEFI/ACPI.
 *      Memory-Mapped I/O (MMIO) regions for PCIe devices appear here (e.g., NVMe BAR,
 *      GPU framebuffer).  The kernel maps these regions with ioremap() (non-cached)
 *      so that device register reads/writes are not buffered by the CPU cache.
 *      Port I/O (PIO) is the older x86 I/O space accessed via in/out instructions;
 *      modern devices use MMIO because it is faster and architecture-independent.
 *
 * WHY: PCIe Gen5 (32 GT/s per lane, ~64 GB/s x16) is standard in 2024 GPU servers.
 *      NVMe SSDs use PCIe Gen4/5 directly -- no SATA/SAS controller overhead.
 *      NVIDIA H100 GPUs use PCIe Gen5 x16 or NVLink 4.0 (900 GB/s bidirectional).
 *      The OS sees the GPU as a PCIe endpoint; its BAR regions appear in /proc/iomem.
 */

static void demo_iomem(void) {
    print_section("Phase 5: Physical Address Space (/proc/iomem)");
    printf("  /proc/iomem shows BIOS/ACPI-assigned physical address regions:\n\n");

    /* OBSERVE: Entries without indentation are top-level regions.
     *          Indented entries are sub-regions of the parent (e.g., PCIe BARs).
     *          'System RAM' entries are usable DRAM; others are device MMIO or reserved. */
    FILE *fp = fopen("/proc/iomem", "r");
    if (!fp) { perror("fopen /proc/iomem"); return; }
    char line[256];
    int count = 0;
    while (fgets(line, sizeof(line), fp) && count < 30) {
        printf("  %s", line);
        count++;
    }
    if (count >= 30) printf("  ... (truncated; see /proc/iomem for full listing)\n");
    fclose(fp);

    printf("\n  OBSERVE: Non-'System RAM' entries are MMIO regions for devices.\n");
    printf("           GPU BAR0/BAR1 (framebuffer + registers) appear here too.\n");
    printf("           PIO: separate 64 KiB I/O port address space (x86 in/out instructions).\n");
    printf("           MMIO: PCIe BARs mapped into physical address space -- faster than PIO.\n");
}

static void demo_cpuinfo_iomem(void) {
    print_section("Phase 6: /proc/cpuinfo Key Fields");
    FILE *fp = fopen("/proc/cpuinfo", "r");
    if (!fp) { perror("fopen /proc/cpuinfo"); return; }
    char line[512];
    int count = 0;
    /* Print first processor block (up to first blank line) */
    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == '\n') { count++; if (count >= 2) break; }
        if (strncmp(line,"processor",9)==0 || strncmp(line,"model name",10)==0 ||
            strncmp(line,"cpu MHz",7)==0 || strncmp(line,"cache size",10)==0 ||
            strncmp(line,"physical id",11)==0 || strncmp(line,"flags",5)==0 ||
            strncmp(line,"bugs",4)==0)
            printf("  %s", line);
    }
    fclose(fp);
    printf("  OBSERVE: 'flags' lists CPU features (avx512, rdrand, hypervisor, etc.)\n");
    printf("           'bugs' lists CPU vulnerabilities requiring SW mitigations\n");
    printf("           (meltdown, spectre_v2, mmio_stale_data, etc.)\n");
}

int main(void) {
    /* EXPECTED OUTPUT (Linux x86_64):
     *   CPU Vendor: GenuineIntel or AuthenticAMD
     *   CPU Brand:  Intel Core i9-... or AMD EPYC ...
     *   AVX-512F: YES (on Xeon/EPYC server), NO (on consumer laptop)
     *   Running in VM: YES (if in cloud/Docker), NO (bare metal)
     *   L1 Data 32K, L1 Instruction 32K, L2 256K-1M, L3 varies by SKU
     *   NUMA: 1 node (laptop), 2+ nodes (dual-socket server)
     *   /proc/iomem: shows System RAM + MMIO regions for devices
     */
    printf("=== Lab 02: PC Architecture ===\n");

    demo_cpuid();
    demo_cache_hierarchy();
    demo_topology();
    demo_memory_info();
    demo_iomem();
    demo_cpuinfo_iomem();

    /* === EXERCISE ===
     * Try these hands-on tasks:
     * 1. Parse /proc/cpuinfo and extract the model name, number of cores, and
     *    the flags field.  Count how many processors (logical CPUs) are listed.
     *    Compare the count against sysconf(_SC_NPROCESSORS_ONLN).
     * 2. Parse /proc/iomem and count how many regions are labeled 'System RAM'
     *    vs MMIO.  Sum the System RAM sizes to compute total physical RAM.
     *    Compare against MemTotal in /proc/meminfo.
     * 3. Modern (NVMe + PCIe Gen5 in GPU servers): identify NVMe or GPU BAR entries
     *    in /proc/iomem (look for PCI lines).  On a server with an H100 GPU, the
     *    GPU framebuffer BAR is typically 80 GiB at a high physical address.
     *    Explain why ACPI must report this BAR so the kernel can ioremap() it.
     *
     * OBSERVE: MMIO BAR regions in /proc/iomem have no 'System RAM' label.
     *          The kernel accesses them via ioremap() with cache-disable attributes
     *          (PAT: Write-Combining for framebuffers, Uncached for control regs).
     * WHY:     MMIO must bypass CPU caches because the device sees writes only when
     *          they reach the PCIe bus.  Write-Combining (WC) batches writes to the
     *          GPU framebuffer for higher throughput without full uncached overhead.
     */
    printf("\n========== Hands-On Exercise ==========\n");
    printf("1. Parse /proc/cpuinfo: count logical CPUs, extract model name and flags.\n");
    printf("2. Parse /proc/iomem: sum 'System RAM' regions and compare to /proc/meminfo MemTotal.\n");
    printf("3. Modern (NVMe/GPU servers): find PCIe BAR entries in /proc/iomem;\n");
    printf("   explain why GPU framebuffer BAR uses Write-Combining (WC) PAT attribute.\n");

    printf("\n========== Quiz ==========\n");
    printf("Q1. What does the CPUID instruction tell you? Why is it useful for performance tuning?\n");
    printf("Q2. Why is L1 cache split into instruction and data caches?\n");
    printf("Q3. What is NUMA and why does it matter for GPU data centre workloads?\n");
    printf("Q4. How does hyperthreading appear in the topology? Do HT siblings share L1?\n");
    printf("Q5. Why would a GPU cluster architect care about cache line size?\n");
    printf("Q6. What is the difference between MMIO and Port I/O (PIO)?  Which does\n");
    printf("    modern PCIe use and why is it faster on current x86 platforms?\n");
    printf("Q7. In a GPU server with PCIe Gen5 x16, what is the theoretical peak\n");
    printf("    bandwidth for a host-to-GPU memcpy, and how does NVLink compare?\n");
    return 0;
}
