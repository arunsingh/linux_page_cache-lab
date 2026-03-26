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

int main(void) {
    printf("=== Lab 02: PC Architecture ===\n");

    demo_cpuid();
    demo_cache_hierarchy();
    demo_topology();
    demo_memory_info();

    printf("\n========== Quiz ==========\n");
    printf("Q1. What does the CPUID instruction tell you? Why is it useful for performance tuning?\n");
    printf("Q2. Why is L1 cache split into instruction and data caches?\n");
    printf("Q3. What is NUMA and why does it matter for GPU data centre workloads?\n");
    printf("Q4. How does hyperthreading appear in the topology? Do HT siblings share L1?\n");
    printf("Q5. Why would a GPU cluster architect care about cache line size?\n");
    return 0;
}
