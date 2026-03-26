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
    /* WHY: Kubernetes pod resource limits use cgroups v2 under the hood.
     *      A Pod's spec.containers[].resources.limits.memory = "4Gi" maps to:
     *        /sys/fs/cgroup/<pod-slice>/memory.max = 4294967296
     *      The kubelet creates a cgroup hierarchy:
     *        /sys/fs/cgroup/kubepods/<qos>/<pod-uid>/<container-cgroup>/
     *      cgroups v2 unified hierarchy means one file tree for ALL controllers
     *      (CPU, memory, I/O, pids) instead of separate per-controller trees (v1).
     *
     * WHY: Firecracker (AWS Lambda) uses KVM microVMs instead of containers for
     *      stronger isolation.  Each Lambda invocation gets its own MicroVM with:
     *      - Isolated kernel (jailer process restricts syscalls via seccomp)
     *      - Virtio devices only (no emulated legacy devices -- faster boot)
     *      - Memory balloon device for dynamic memory reclaim between invocations
     *      Boot time: <125 ms for a minimal microVM (vs seconds for full VM).
     *      AWS Fargate (container service) also uses Firecracker microVMs.
     */
    ps("Phase 5: Reading cgroup Memory Limits and Kubernetes Context");
    printf("  Reading cgroup v2 memory limits for this process:\n");
    {
        /* Show cgroup path */
        FILE *fp2 = fopen("/proc/self/cgroup", "r");
        char cgpath[512] = "";
        if (fp2) {
            char line[256];
            while (fgets(line, sizeof(line), fp2)) {
                /* cgroups v2: line is "0::<path>" */
                if (strncmp(line, "0::", 3) == 0) {
                    strncpy(cgpath, line + 3, sizeof(cgpath)-1);
                    cgpath[strcspn(cgpath, "\n")] = '\0';
                    printf("  cgroup v2 path: %s\n", cgpath);
                }
            }
            fclose(fp2);
        }

        /* Try to read memory.max from our cgroup */
        char fullpath[640];
        snprintf(fullpath, sizeof(fullpath), "/sys/fs/cgroup%s/memory.max", cgpath);
        FILE *mem = fopen(fullpath, "r");
        if (!mem) mem = fopen("/sys/fs/cgroup/memory.max", "r");
        if (mem) {
            char val[64] = {0};
            if (fgets(val, sizeof(val), mem)) {
                val[strcspn(val, "\n")] = '\0';
                if (strcmp(val, "max") == 0) {
                    printf("  memory.max: max (unlimited -- not in a limited container)\n");
                } else {
                    long bytes = atol(val);
                    printf("  memory.max: %s bytes (~%.0f MiB)\n", val, bytes / (1024.0*1024.0));
                }
            }
            fclose(mem);
        }

        /* Show pids.max if available */
        snprintf(fullpath, sizeof(fullpath), "/sys/fs/cgroup%s/pids.max", cgpath);
        FILE *pids = fopen(fullpath, "r");
        if (!pids) pids = fopen("/sys/fs/cgroup/pids.max", "r");
        if (pids) {
            char val[32] = {0};
            if (fgets(val, sizeof(val), pids)) {
                val[strcspn(val, "\n")] = '\0';
                printf("  pids.max: %s (container fork-bomb protection)\n", val);
            }
            fclose(pids);
        }
    }

    printf("\n  Kubernetes pod cgroup hierarchy:\n");
    printf("    /sys/fs/cgroup/kubepods/<qos>/<pod-uid>/<container>/\n");
    printf("    QoS classes: Guaranteed (limits==requests), Burstable, BestEffort\n");
    printf("    Guaranteed pods: CPU pinned, no memory balloon, first served by scheduler\n\n");
    printf("  Firecracker microVM (AWS Lambda):\n");
    printf("    - KVM microVM with minimal virtio devices, jailer seccomp, memory balloon\n");
    printf("    - <125ms boot time (vs seconds for QEMU full VM)\n");
    printf("    - Each Lambda function invocation = isolated microVM\n");
    printf("    - No shared kernel between Lambda functions (stronger than containers)\n");

    /* === EXERCISE ===
     * Try these hands-on tasks:
     * 1. Read the full cgroup v2 hierarchy for this process:
     *      cat /proc/self/cgroup           (shows cgroup path)
     *      ls /sys/fs/cgroup/<path>/        (shows all available controllers)
     *      cat /sys/fs/cgroup/<path>/memory.current  (current RSS in bytes)
     *    Compare memory.current to VmRSS in /proc/self/status.
     * 2. Simulate a Kubernetes memory limit in a shell:
     *      mkdir /sys/fs/cgroup/mytest
     *      echo 50000000 > /sys/fs/cgroup/mytest/memory.max   (50 MB limit)
     *      echo $$ > /sys/fs/cgroup/mytest/cgroup.procs        (add this shell)
     *    Then try to malloc more than 50 MB in C and observe OOM kill.
     *    Clean up: echo 1 > /sys/fs/cgroup/mytest/cgroup.kill; rmdir /sys/fs/cgroup/mytest
     * 3. Modern (Firecracker microVMs): compare container vs microVM isolation:
     *    Container: shared kernel (PID ns, NET ns, cgroup isolation)
     *    Firecracker: separate kernel per workload (KVM, no shared kernel attack surface)
     *    Read: https://github.com/firecracker-microvm/firecracker
     *    Explain: why does Firecracker improve on container security for multi-tenant Lambda?
     *
     * OBSERVE: cgroups v2 unified hierarchy means one cgroup.procs file controls all
     *          subsystems.  In v1, you had to join each subsystem separately:
     *          /sys/fs/cgroup/memory/<group>/tasks, /sys/fs/cgroup/cpu/<group>/tasks, etc.
     *          v2 atomically enforces all limits together -- no partial-join state.
     * WHY:     Kubernetes prefers cgroups v2 because it fixes a v1 bug where a process
     *          could escape memory accounting by forking into a different cgroup subtree.
     *          v2's "no-internal-process" rule prevents processes from living in non-leaf
     *          cgroup nodes, making memory accounting consistent.
     */
    printf("\n========== Hands-On Exercise ==========\n");
    printf("1. Read /sys/fs/cgroup/<path>/memory.current; compare to /proc/self/status VmRSS.\n");
    printf("2. Simulate K8s memory limit: mkdir cgroup, set memory.max=50MB,\n");
    printf("   add shell process, try to malloc > limit and observe OOM kill.\n");
    printf("3. Modern (Firecracker): compare container vs microVM isolation model;\n");
    printf("   explain why separate kernel per workload improves multi-tenant security.\n");

    printf("\n========== Quiz ==========\n");
    printf("Q1. How do cgroups enforce a memory limit? What happens on OOM?\n");
    printf("Q2. Why cant you see host processes from inside a PID namespace?\n");
    printf("Q3. What is overlayfs and why is it efficient for container images?\n");
    printf("Q4. How does Kubernetes schedule GPU workloads across nodes?\n");
    printf("Q5. What is the difference between cgroups v1 and v2?  Why does Kubernetes\n");
    printf("    prefer v2 for memory accounting?\n");
    printf("Q6. What is Firecracker and how does it differ from Docker containers\n");
    printf("    in terms of kernel sharing and isolation?\n");
    printf("Q7. What are Kubernetes QoS classes (Guaranteed/Burstable/BestEffort)\n");
    printf("    and how do they map to cgroup priority and OOM score?\n");
    return 0;
}
