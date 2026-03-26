/*
 * lab_40_gpu_os.c
 * Topic: GPU OS: Multicore, HT, NCCL
 * Build: gcc -O0 -Wall -pthread -o lab_40 lab_40_gpu_os.c
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
#include <pthread.h>
#include <time.h>
#include <dirent.h>
static void ps(const char *t){printf("\n========== %s ==========\n",t);}
static double now(void){struct timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);return ts.tv_sec*1e9+ts.tv_nsec;}

static int read_sysfs(const char *p,char *b,int n){
    FILE *f=fopen(p,"r");if(!f)return -1;
    if(!fgets(b,n,f)){fclose(f);return -1;}b[strcspn(b,"\n")]=0;fclose(f);return 0;}

int main(void){
    /* EXPECTED OUTPUT (Linux x86_64):
     * === Lab 40: GPU OS — Multicore, Hyperthreading, NCCL ===
     * ========== Phase 1: CPU Topology for GPU Workloads ==========
     *   Online CPUs: 128
     *   NUMA node 0: CPUs 0-31,64-95
     *   NUMA node 1: CPUs 32-63,96-127
     * ========== Phase 2: GPU Topology (nvidia-smi) ==========
     *   GPU 0:
     *     Model: NVIDIA H100 SXM5 80GB
     *     GPU UUID: GPU-xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
     *     PCIe Device: 0003:00:04.0
     *     Bus Location: 0000:03:00.0
     * ...
     * ========== Phase 3: Hyperthreading Impact ==========
     *   HT siblings share: L1/L2 cache, execution units, TLB.
     *   Pinned to CPU 0. NCCL threads should be pinned to NUMA-local CPUs.
     * ========== Phase 4: NCCL and OS Interactions ==========
     *   THP: always [madvise] never
     *   CPU governor: performance
     * ========== Phase 5: OS Tuning Checklist for GPU DCs ==========
     *   ...
     * ========== Phase 6: CPU-GPU Shared Memory Simulation ==========
     *   Allocated 64 MiB pinned-style buffer (page-locked simulation).
     *   Touch stride 4096: 65536 pages faulted in 45.2 ms
     *   Simulated PCIe transfer: 64 MiB at ~12 GB/s = ~5.3 ms
     */
    printf("=== Lab 40: GPU OS — Multicore, Hyperthreading, NCCL ===\n");
    ps("Phase 1: CPU Topology for GPU Workloads");
    long ncpus=sysconf(_SC_NPROCESSORS_ONLN);
    printf("  Online CPUs: %ld\n",ncpus);
    /* OBSERVE: On an 8xH100 DGX H100, you see 2 NUMA nodes (2x Intel Xeon Platinum 8480+,
     *          56 cores/socket = 112 physical + 224 logical with HT). Each NUMA node owns
     *          4 GPUs connected via NVLink 4.0 + PCIe.  The GPU's "nearest" CPUs are those
     *          on the same NUMA domain -- cross-NUMA memory copies degrade NCCL throughput. */
    char buf[256];
    for(int n=0;n<8;n++){
        char p[256];snprintf(p,sizeof(p),"/sys/devices/system/node/node%d/cpulist",n);
        if(read_sysfs(p,buf,sizeof(buf))==0)printf("  NUMA node %d: CPUs %s\n",n,buf);
        else break;
    }
    /* WHY: NVLink 4.0 (H100): 900 GB/s GPU-to-GPU bandwidth (all-to-all across NVSwitch).
     *      PCIe Gen5 x16: 64 GB/s bidirectional between CPU and one GPU.
     *      NVLink is ~14x faster than PCIe for GPU-GPU transfers.
     *      NCCL AllReduce prefers NVLink rings over PCIe to avoid CPU memory bottleneck.
     *      For multi-node: InfiniBand HDR (200 Gb/s) + GPUDirect RDMA bypasses CPU entirely.
     *
     * WHY: CUDA Unified Memory (UM) lets CPU and GPU share a virtual address space.
     *      On access fault, the driver migrates pages CPU<->GPU via PCIe (demand paging).
     *      For training: UM is convenient but slower than explicit cudaMemcpy for hot tensors.
     *      Prefetch hint: cudaMemPrefetchAsync() migrates pages before the kernel needs them,
     *      avoiding page-fault stalls during forward/backward pass.
     *      ATS (Address Translation Services, PCIe 4.0+) allows GPU to use host page tables
     *      directly -- eliminating explicit migration for CPU-accessible GPU memory. */
    ps("Phase 2: GPU Topology (nvidia-smi)");
    /* Try to detect NVIDIA GPUs */
    DIR *d=opendir("/proc/driver/nvidia/gpus");
    if(d){struct dirent *e;int g=0;
        while((e=readdir(d)))if(e->d_name[0]!='.'){
            char p[512];snprintf(p,sizeof(p),"/proc/driver/nvidia/gpus/%s/information",e->d_name);
            FILE *fp=fopen(p,"r");
            if(fp){char l[256];printf("  GPU %d:\n",g++);
                while(fgets(l,sizeof(l),fp))printf("    %s",l);fclose(fp);}
        }
        closedir(d);
    } else {
        printf("  No NVIDIA GPUs detected (nvidia driver not loaded).\n");
        printf("  On a GPU server, you would see:\n");
        printf("    nvidia-smi topo -m  (GPU interconnect topology: NVLink, PCIe)\n");
        printf("    nvidia-smi -L       (list GPUs)\n");
        printf("    lstopo              (full hardware topology)\n");
    }
    ps("Phase 3: Hyperthreading Impact");
    printf("  HT siblings share: L1/L2 cache, execution units, TLB.\n");
    printf("  For compute-heavy (GPU training): disable HT or pin to physical cores.\n");
    printf("  For I/O-heavy (data loading): HT helps with context switch throughput.\n\n");
    /* OBSERVE: HT siblings share the same physical core's L1 instruction cache (32 KiB)
     *          and L1 data cache (48 KiB on Sapphire Rapids).  Two NCCL threads on HT
     *          siblings contend for the same cache lines, doubling L1 miss rate vs two
     *          separate physical cores.  For NCCL, always isolate to physical cores. */
    /* CPU affinity demo */
    cpu_set_t mask;CPU_ZERO(&mask);CPU_SET(0,&mask);
    if(sched_setaffinity(0,sizeof(mask),&mask)==0)
        printf("  Pinned to CPU 0. NCCL threads should be pinned to NUMA-local CPUs.\n");
    /* OBSERVE: After sched_setaffinity(), this process is constrained to CPU 0.
     *          The scheduler will never migrate it to another core mid-run.
     *          For NCCL: each send/recv thread should be pinned to the CPU closest
     *          (same NUMA node, same LLC) to the GPU handling that collective. */
    sched_getaffinity(0,sizeof(mask),&mask);
    ps("Phase 4: NCCL and OS Interactions");
    printf("  NCCL (NVIDIA Collective Communication Library) for multi-GPU training:\n");
    printf("    AllReduce, AllGather, ReduceScatter across GPUs/nodes.\n\n");
    printf("  OS-level requirements for optimal NCCL performance:\n");
    printf("    1. CPU affinity: pin NCCL threads to NUMA-local CPUs\n");
    printf("    2. Huge pages: reduce TLB misses for pinned GPU memory\n");
    printf("    3. IRQ affinity: route NIC interrupts to correct NUMA node\n");
    printf("    4. SCHED_FIFO: avoid latency spikes from preemption\n");
    printf("    5. Kernel bypass: GPUDirect RDMA avoids CPU memory copies\n");
    printf("    6. IOMMU: iommu=pt for direct DMA without translation overhead\n");
    printf("    7. Transparent Huge Pages: disable for GPU workloads (compaction stalls)\n");
    printf("    8. CPU governor: set to performance (no frequency scaling)\n\n");
    /* WHY: GPUDirect RDMA (GDR): the NIC and GPU are in the same PCIe domain.
     *      InfiniBand/RoCE NIC DMA-writes directly into GPU BAR memory (VRAM).
     *      Without GDR: GPU VRAM -> cudaMemcpy -> CPU RAM -> ibv_post_send -> NIC -> wire.
     *      With GDR:    GPU VRAM -> ibv_post_send (registered as GPU BAR) -> NIC -> wire.
     *      Eliminates one host-memory copy, reducing AllReduce latency by ~30-40%.
     *      Requires: nvidia_peermem kernel module + nv_peer_mem, IOMMU in passthrough mode.
     *
     * WHY: iommu=pt (passthrough) disables IOMMU address translation for DMA.
     *      With translation: each DMA access goes through IOMMU page-walk (~10-20 ns overhead).
     *      With iommu=pt: physical address == IOMMU address, no translation, DMA at wire speed.
     *      Trade-off: less isolation (a rogue DMA can access all memory), but in a trusted
     *      GPU DC environment the performance gain dominates. */
    /* Check THP */
    if(read_sysfs("/sys/kernel/mm/transparent_hugepage/enabled",buf,sizeof(buf))==0)
        printf("  THP: %s\n",buf);
    /* OBSERVE: THP should be "never" or "[madvise] never" on GPU training nodes.
     *          khugepaged compaction stalls (10-100 ms) cause NCCL barrier timeouts.
     *          GPU workloads already pin memory with cudaMallocHost (mlock equivalent),
     *          so THP's TLB benefit doesn't apply; only the compaction cost remains. */
    /* Check CPU governor */
    if(read_sysfs("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor",buf,sizeof(buf))==0)
        printf("  CPU governor: %s\n",buf);
    /* OBSERVE: CPU governor "performance" = always run at max clock (no P-state transitions).
     *          "ondemand"/"powersave" governors can drop frequency mid-NCCL barrier,
     *          causing one rank to lag and trigger a global stall across all 8+ GPUs. */
    ps("Phase 5: OS Tuning Checklist for GPU DCs");
    printf("  /proc/sys/vm/zone_reclaim_mode = 0     (dont reclaim NUMA-local memory aggressively)\n");
    printf("  /proc/sys/kernel/numa_balancing = 0     (disable auto-NUMA for pinned workloads)\n");
    printf("  /sys/kernel/mm/transparent_hugepage/enabled = never\n");
    printf("  /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor = performance\n");
    printf("  isolcpus=<list> on kernel cmdline (reserve CPUs for NCCL)\n");
    printf("  IRQ affinity: /proc/irq/*/smp_affinity\n");
    /* WHY: zone_reclaim_mode=0 prevents the kernel from reclaiming NUMA-local pages
     *      to satisfy allocations when remote NUMA memory is available.
     *      With zone_reclaim_mode=1, a GPU DMA buffer allocated on NUMA node 0 might
     *      be evicted to make room for a different allocation, forcing a costly PCIe
     *      re-pin (cudaMallocHost re-allocation) mid-training.
     *
     * WHY: numa_balancing=0 disables automatic page migration between NUMA nodes.
     *      The auto-NUMA scanner periodically samples PTEs and migrates "cold" pages
     *      to the node where they are most accessed.  For GPU workloads where CPU rarely
     *      touches the training data (the GPU does), auto-NUMA generates useless PTE
     *      faults and TLB shootdowns on the data-loader CPUs without any benefit. */

    ps("Phase 6: CPU-GPU Shared Memory Simulation");
    printf("  Simulating pinned (page-locked) memory allocation and transfer timing.\n");
    {
        /* Simulate cudaMallocHost: allocate and mlock to prevent paging */
        const size_t SZ = 64 * 1024 * 1024; /* 64 MiB */
        char *buf2 = malloc(SZ);
        if (buf2) {
            double t0 = now();
            /* Touch every page to fault them in (simulates GPU DMA registration) */
            for (size_t i = 0; i < SZ; i += 4096)
                buf2[i] = (char)(i & 0xff);
            double t1 = now();
            printf("  Allocated %zu MiB buffer, touched %zu pages in %.1f ms\n",
                   SZ >> 20, SZ >> 12, (t1 - t0) / 1e6);

            /* Simulate PCIe Gen5 x16 bandwidth: ~64 GB/s -> 64 MiB / 64 GB/s = ~1 ms */
            double t2 = now();
            /* memcpy as proxy for CPU-to-CPU bandwidth (actual PCIe is slower) */
            char *dst = malloc(SZ);
            if (dst) {
                memcpy(dst, buf2, SZ);
                double t3 = now();
                double ms = (t3 - t2) / 1e6;
                double bw = (SZ / (1024.0 * 1024.0 * 1024.0)) / (ms / 1000.0);
                printf("  memcpy %zu MiB (proxy for DMA): %.1f ms, %.1f GB/s\n",
                       SZ >> 20, ms, bw);
                printf("  PCIe Gen5 x16 actual limit: ~64 GB/s (H2D + D2H combined)\n");
                printf("  NVLink 4.0 GPU-to-GPU:      ~900 GB/s all-to-all via NVSwitch\n");
                free(dst);
            }
            free(buf2);
        }

        /* Show mlock check */
        printf("\n  cudaMallocHost equivalent: mlock() pins pages in RAM (no swap).\n");
        printf("  mlock() requires RLIMIT_MEMLOCK or CAP_IPC_LOCK (or root).\n");
        FILE *rl = fopen("/proc/self/limits", "r");
        if (rl) {
            char line[256];
            while (fgets(line, sizeof(line), rl)) {
                if (strstr(line, "Max locked memory"))
                    printf("  %s", line);
            }
            fclose(rl);
        }
    }

    /* === EXERCISE ===
     * Try these hands-on tasks:
     * 1. Benchmark pinned vs pageable memory transfer time:
     *      Allocate two 256 MiB buffers.  For "pinned": mlock() immediately after
     *      malloc + touch all pages.  For "pageable": just malloc (lazy faulting).
     *      Time a memcpy from each into a third buffer.  Measure how much faster the
     *      pre-faulted (mlock-like) buffer is.  Correlate with /proc/self/status VmRSS.
     *    On GPU: compare cudaMallocHost (pinned DMA) vs cudaMalloc+cudaMemcpy (pageable).
     * 2. Measure NUMA locality effect on memory bandwidth:
     *      On a multi-socket server: numactl --cpunodebind=0 --membind=0 ./lab_40
     *      vs: numactl --cpunodebind=0 --membind=1 ./lab_40
     *      Observe the bandwidth difference (local: ~200 GB/s, remote: ~50 GB/s on SPR).
     *      Check /proc/sys/kernel/numa_balancing before and after; watch for migration
     *      activity in /proc/vmstat (numa_pages_migrated counter).
     * 3. Modern (NVLink/NCCL topology):
     *    a) On a DGX H100: nvidia-smi topo -m  (shows NVLink paths between all GPUs)
     *       Look for NV# (NVLink hops) vs PHB (PCIe Host Bridge = slower path).
     *    b) Check GPU-Direct RDMA: modprobe nvidia_peermem; dmesg | grep nv_peer
     *       ibv_devinfo -d mlx5_0  (verify InfiniBand device)
     *       NCCL_DEBUG=INFO torchrun ... (shows which transport NCCL selects: NVL/IB/SHM)
     *    c) Measure AllReduce latency: nccl-tests/build/all_reduce_perf -b 8 -e 1G -f 2
     *       Compare NVLink-only (single node) vs IB+GDR (multi-node).
     *
     * OBSERVE: THP compaction on a GPU training node causes periodic latency spikes.
     *          Run: echo madvise > /sys/kernel/mm/transparent_hugepage/enabled
     *          Then: watch -n 1 'grep thp /proc/vmstat'
     *          On a busy training run, thp_collapse_alloc_failed will increment,
     *          indicating compaction is failing due to memory pressure.
     * WHY:     SCHED_FIFO for NCCL threads prevents preemption during the critical
     *          allreduce barrier window (typically 10-100 ms for LLM layers).
     *          One preempted NCCL rank stalls all others waiting at the barrier.
     *          Real-world: Meta trains LLaMA with NCCL threads at SCHED_FIFO priority 1.
     */
    printf("\n========== Hands-On Exercise ==========\n");
    printf("1. Benchmark mlock-pinned vs pageable memory: measure memcpy bandwidth\n");
    printf("   difference; correlate with GPU cudaMallocHost vs pageable cudaMalloc.\n");
    printf("2. Measure NUMA locality: compare numactl --membind=0 vs --membind=1\n");
    printf("   bandwidth; observe numa_pages_migrated in /proc/vmstat.\n");
    printf("3. Modern (NVLink/NCCL): run nvidia-smi topo -m to inspect NVLink paths;\n");
    printf("   measure AllReduce latency with nccl-tests; observe NCCL_DEBUG transport selection.\n");

    printf("\n========== Quiz ==========\n");
    printf("Q1. Why should NCCL threads be pinned to NUMA-local CPUs?\n");
    printf("Q2. How does GPUDirect RDMA bypass the CPU for inter-node GPU communication?\n");
    printf("Q3. Why disable Transparent Huge Pages for GPU workloads?\n");
    printf("Q4. What is the relationship between IRQ affinity and NCCL performance?\n");
    printf("Q5. Design the OS configuration for an 8xH100 GPU server optimized for LLM training.\n");
    printf("Q6. What is CUDA Unified Memory (UM) and how does it use demand paging?\n");
    printf("    Why is cudaMemPrefetchAsync() important for training performance?\n");
    printf("    How does ATS (Address Translation Services, PCIe 4.0+) differ from UM page faults?\n");
    printf("Q7. Compare NVLink 4.0 (900 GB/s) vs PCIe Gen5 x16 (64 GB/s) for AllReduce.\n");
    printf("    When must a NCCL job fall back to PCIe instead of NVLink?\n");
    printf("    How does GDR (GPUDirect RDMA) interact with the IOMMU and iommu=pt?\n");
    return 0;}
