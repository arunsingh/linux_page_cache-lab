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
    printf("=== Lab 40: GPU OS — Multicore, Hyperthreading, NCCL ===\n");
    ps("Phase 1: CPU Topology for GPU Workloads");
    long ncpus=sysconf(_SC_NPROCESSORS_ONLN);
    printf("  Online CPUs: %ld\n",ncpus);
    /* Show NUMA-GPU affinity */
    char buf[256];
    for(int n=0;n<8;n++){
        char p[256];snprintf(p,sizeof(p),"/sys/devices/system/node/node%d/cpulist",n);
        if(read_sysfs(p,buf,sizeof(buf))==0)printf("  NUMA node %d: CPUs %s\n",n,buf);
        else break;
    }
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
    /* CPU affinity demo */
    cpu_set_t mask;CPU_ZERO(&mask);CPU_SET(0,&mask);
    if(sched_setaffinity(0,sizeof(mask),&mask)==0)
        printf("  Pinned to CPU 0. NCCL threads should be pinned to NUMA-local CPUs.\n");
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
    /* Check THP */
    if(read_sysfs("/sys/kernel/mm/transparent_hugepage/enabled",buf,sizeof(buf))==0)
        printf("  THP: %s\n",buf);
    /* Check CPU governor */
    if(read_sysfs("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor",buf,sizeof(buf))==0)
        printf("  CPU governor: %s\n",buf);
    ps("Phase 5: OS Tuning Checklist for GPU DCs");
    printf("  /proc/sys/vm/zone_reclaim_mode = 0     (dont reclaim NUMA-local memory aggressively)\n");
    printf("  /proc/sys/kernel/numa_balancing = 0     (disable auto-NUMA for pinned workloads)\n");
    printf("  /sys/kernel/mm/transparent_hugepage/enabled = never\n");
    printf("  /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor = performance\n");
    printf("  isolcpus=<list> on kernel cmdline (reserve CPUs for NCCL)\n");
    printf("  IRQ affinity: /proc/irq/*/smp_affinity\n");
    printf("\n========== Quiz ==========\n");
    printf("Q1. Why should NCCL threads be pinned to NUMA-local CPUs?\n");
    printf("Q2. How does GPUDirect RDMA bypass the CPU for inter-node GPU communication?\n");
    printf("Q3. Why disable Transparent Huge Pages for GPU workloads?\n");
    printf("Q4. What is the relationship between IRQ affinity and NCCL performance?\n");
    printf("Q5. Design the OS configuration for an 8xH100 GPU server optimized for LLM training.\n");
    return 0;}
