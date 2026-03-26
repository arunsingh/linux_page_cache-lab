/*
 * lab_39_cloud_compute.c
 * Topic: Cloud Computing
 * Build: gcc -O0 -Wall -pthread -o lab_39 lab_39_cloud_compute.c
 *
 * Author: Arun Singh | arunsingh.in@gmail.com
 * OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
*/
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sched.h>
#include <sys/stat.h>
static void ps(const char *t){printf("\n========== %s ==========\n",t);}
int main(void){
    printf("=== Lab 39: Cloud Computing OS Primitives ===\n");
    ps("Phase 1: Cgroups — Resource Control");
    printf("  Cgroups v2 hierarchy: /sys/fs/cgroup/\n");
    printf("  Controls: cpu, memory, io, pids, cpuset\n\n");
    /* Show current cgroup */
    FILE *fp=fopen("/proc/self/cgroup","r");
    if(fp){char l[256];while(fgets(l,sizeof(l),fp))printf("  %s",l);fclose(fp);}
    printf("\n  Memory cgroup enforces limits:\n");
    fp=fopen("/sys/fs/cgroup/memory.max","r");
    if(!fp)fp=fopen("/sys/fs/cgroup/memory/memory.limit_in_bytes","r");
    if(fp){char l[64];if(fgets(l,sizeof(l),fp))printf("    memory.max: %s",l);fclose(fp);}
    ps("Phase 2: Namespaces — Isolation");
    printf("  This process namespaces:\n");
    const char *ns[]={"cgroup","ipc","mnt","net","pid","user","uts",NULL};
    for(int i=0;ns[i];i++){
        char p[128];struct stat st;
        snprintf(p,sizeof(p),"/proc/self/ns/%s",ns[i]);
        if(stat(p,&st)==0)printf("    %s: inode=%lu\n",ns[i],st.st_ino);
    }
    ps("Phase 3: Container = cgroups + namespaces + fs overlay");
    printf("  Docker/containerd builds on:\n");
    printf("    cgroups: resource limits (CPU, memory, I/O)\n");
    printf("    namespaces: isolation (PID, network, mount, user)\n");
    printf("    overlayfs: layered filesystem (image layers)\n");
    printf("    seccomp: syscall filtering\n");
    printf("    capabilities: fine-grained privileges\n");
    ps("Phase 4: Cloud-Native GPU Scheduling");
    printf("  Kubernetes GPU scheduling:\n");
    printf("    nvidia-device-plugin exposes GPU resources\n");
    printf("    Pod requests: nvidia.com/gpu: 1\n");
    printf("    GPU isolation: CUDA_VISIBLE_DEVICES + MIG\n");
    printf("    Scheduling constraints: NUMA locality, PCIe topology\n");
    printf("\n========== Quiz ==========\n");
    printf("Q1. How do cgroups enforce a memory limit? What happens on OOM?\n");
    printf("Q2. Why cant you see host processes from inside a PID namespace?\n");
    printf("Q3. What is overlayfs and why is it efficient for container images?\n");
    printf("Q4. How does Kubernetes schedule GPU workloads across nodes?\n");
    return 0;}
