/*
 * lab_38_virtualization.c
 * Topic: Virtualization
 * Build: gcc -O0 -Wall -pthread -o lab_38 lab_38_virtualization.c
 *
 * Author: Arun Singh | arunsingh.in@gmail.com
 * OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
*/
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
static void ps(const char *t){printf("\n========== %s ==========\n",t);}
int main(void){
    printf("=== Lab 38: Virtualization ===\n");
    ps("Phase 1: Hypervisor Detection");
#if defined(__x86_64__)
    unsigned eax,ebx,ecx,edx;
    __asm__ volatile("cpuid":"=a"(eax),"=b"(ebx),"=c"(ecx),"=d"(edx):"a"(1));
    int in_vm=ecx&(1<<31);
    printf("  CPUID hypervisor bit: %s\n",in_vm?"SET (running in VM)":"NOT SET (bare metal likely)");
    if(in_vm){
        __asm__ volatile("cpuid":"=a"(eax),"=b"(ebx),"=c"(ecx),"=d"(edx):"a"(0x40000000));
        char hv[13]={0};
        memcpy(hv,&ebx,4);memcpy(hv+4,&ecx,4);memcpy(hv+8,&edx,4);
        printf("  Hypervisor: %s\n",hv);
    }
#endif
    ps("Phase 2: Virtualization Types");
    printf("  Type 1 (bare-metal): KVM, Xen, VMware ESXi — hypervisor on hardware.\n");
    printf("  Type 2 (hosted): VirtualBox, VMware Workstation — hypervisor on host OS.\n");
    printf("  Paravirtualization: guest OS modified for hypervisor (Xen PV, virtio).\n");
    printf("  Hardware-assisted: Intel VT-x, AMD-V — CPU supports VM natively.\n");
    ps("Phase 3: KVM Architecture");
    printf("  KVM turns Linux into a Type-1 hypervisor:\n");
    printf("    /dev/kvm — userspace interface\n");
    printf("    QEMU provides device emulation\n");
    printf("    Guest runs in a special CPU mode (non-root mode)\n");
    printf("    VM exits trap to KVM for privileged operations\n\n");
    FILE *fp=fopen("/dev/kvm","r");
    if(fp){printf("  /dev/kvm present — KVM available.\n");fclose(fp);}
    else printf("  /dev/kvm not present (may need modprobe kvm).\n");
    ps("Phase 4: GPU Virtualization");
    printf("  NVIDIA vGPU: time-sliced GPU sharing (MIG on A100/H100).\n");
    printf("  GPU passthrough: full GPU assigned to one VM (SR-IOV).\n");
    printf("  MIG: Multi-Instance GPU — hardware-partitioned GPU slices.\n");
    /* WHY: VFIO (Virtual Function I/O) for GPU passthrough:
     *      VFIO lets a userspace process (QEMU) own a PCI device directly.
     *      The IOMMU creates a separate DMA address space for the device.
     *      GPU DMA to "guest physical" addresses is translated by the IOMMU to
     *      host physical addresses -- providing isolation without full device emulation.
     *      This is how bare-metal GPU performance is achieved in VMs.
     *      VFIO requires: IOMMU groups, ACS (Access Control Services) on PCIe switches.
     *
     * WHY: KVM on ARM vs x86:
     *      x86 VT-x: VMENTER/VMEXIT instructions, VMCS (VM Control Structure) per vCPU.
     *      ARM: EL2 (hypervisor exception level), VTCR_EL2 for stage-2 page tables.
     *      Both achieve hardware-accelerated virtualization with stage-2 address translation
     *      (guest physical -> host physical translation done by hardware IOMMU/MMU).
     */
    ps("Phase 5: VFIO GPU Passthrough and VM Detection");
    printf("  VFIO (Virtual Function I/O):\n");
    printf("    - IOMMU-based device isolation for userspace direct device access\n");
    printf("    - GPU assigned to VM: IOMMU translates GPU DMA to host physical addresses\n");
    printf("    - /dev/vfio/<group>: VFIO group device file\n");
    printf("    - Used by: QEMU GPU passthrough, DPDK, SPDK\n\n");

    /* Check for VFIO */
    FILE *fp2 = fopen("/dev/vfio/vfio", "r");
    if (fp2) {
        printf("  /dev/vfio/vfio present -- VFIO module loaded\n");
        fclose(fp2);
    } else {
        printf("  /dev/vfio/vfio not present (modprobe vfio vfio_pci to enable)\n");
    }

    /* Check IOMMU groups */
    FILE *fp3 = popen("ls /sys/kernel/iommu_groups/ 2>/dev/null | wc -l", "r");
    if (fp3) {
        char val[16] = {0};
        if (fgets(val, sizeof(val), fp3)) {
            val[strcspn(val, "\n")] = '\0';
            printf("  IOMMU groups: %s (0 = IOMMU disabled or passthrough mode)\n", val);
        }
        pclose(fp3);
    }

    printf("\n  GPU virtualization comparison:\n");
    printf("    Time-sliced vGPU: GPU context-switched like CPU (NVIDIA GRID)\n");
    printf("                      Multiple VMs share one GPU; each gets time slice\n");
    printf("                      Latency: same as bare-metal / context-switch overhead\n");
    printf("    MIG (A100/H100):  hardware-partitioned GPU slices (1/7, 2/7, 4/7, etc.)\n");
    printf("                      Each slice is isolated: VRAM, cache, compute engines\n");
    printf("                      No inter-slice interference (unlike time-slicing)\n");
    printf("    VFIO passthrough:  entire GPU to one VM; full VRAM and compute\n");
    printf("                      Best performance; no sharing\n");

    /* === EXERCISE ===
     * Try these hands-on tasks:
     * 1. Detect the hypervisor type:
     *    a) CPUID leaf 0x40000000: hypervisor vendor string (KVM, "KVMKVMKVM")
     *    b) /proc/cpuinfo flags: check for 'hypervisor' flag
     *    c) dmesg | grep 'Hypervisor\|KVM\|VMware\|Xen\|Hyper-V'
     *    d) systemd-detect-virt (if installed)
     *    Compare the detection methods and their reliability.
     * 2. Measure VM exit overhead:
     *    In a KVM guest, run a loop calling RDTSC (legitimate, no VM exit) vs
     *    calling cpuid (may cause VM exit).  Measure the latency difference.
     *    cpuid in a VM: ~1-5 us (VM exit + handler + VM entry)
     *    RDTSC in a VM: ~10-50 ns (may be intercepted via TSC offsetting)
     * 3. Modern (VFIO GPU passthrough): on a server with SR-IOV capable GPU,
     *    list IOMMU groups: ls /sys/kernel/iommu_groups/*/devices/
     *    Verify each GPU is in its own IOMMU group (required for safe passthrough).
     *    Explain: why does the IOMMU need ACS (PCIe Access Control Services) enabled
     *    to isolate GPU DMA from other devices in the same PCIe switch?
     *
     * OBSERVE: The hypervisor bit (CPUID.1:ECX bit 31) is set by KVM, VMware, Xen, Hyper-V.
     *          Bare metal: this bit is always 0.  Cloud VMs: always set.
     *          This is how jemalloc, Java GC, and databases detect virtual environments
     *          and adjust their behavior (e.g., disable huge page hinting in VMs).
     * WHY:     VM exits are expensive (>1 us) because they require flushing pipeline,
     *          saving/restoring vmcs (4KB structure), and potentially TLB shootdowns.
     *          virtio reduces VM exits by batching device notifications via shared memory
     *          rings, avoiding one exit per I/O operation.
     */
    printf("\n========== Hands-On Exercise ==========\n");
    printf("1. Detect hypervisor via CPUID, /proc/cpuinfo flags, and dmesg;\n");
    printf("   compare reliability of each method.\n");
    printf("2. Measure VM exit overhead: compare CPUID latency vs RDTSC in a KVM guest.\n");
    printf("3. Modern (VFIO passthrough): list IOMMU groups for GPUs;\n");
    printf("   explain why ACS (PCIe Access Control Services) is required for safe isolation.\n");

    printf("\n========== Quiz ==========\n");
    printf("Q1. What is a VM exit and when does it occur?\n");
    printf("Q2. How does KVM use Intel VT-x for hardware virtualization?\n");
    printf("Q3. What is virtio and why is it faster than emulated devices?\n");
    printf("Q4. How does MIG differ from time-sliced vGPU sharing?\n");
    printf("Q5. What is VFIO and how does it enable GPU passthrough to VMs?\n");
    printf("    What role does the IOMMU play in isolating GPU DMA?\n");
    printf("Q6. On a KVM host, what is the overhead of a VM exit caused by CPUID\n");
    printf("    vs a RDTSC that is handled without a VM exit?\n");
    printf("Q7. What is SR-IOV (Single Root I/O Virtualization) and how does it differ\n");
    printf("    from MIG for GPU sharing across multiple VMs or Kubernetes pods?\n");
    return 0;
}
