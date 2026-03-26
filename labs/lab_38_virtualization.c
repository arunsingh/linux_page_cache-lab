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
    printf("\n========== Quiz ==========\n");
    printf("Q1. What is a VM exit and when does it occur?\n");
    printf("Q2. How does KVM use Intel VT-x for hardware virtualization?\n");
    printf("Q3. What is virtio and why is it faster than emulated devices?\n");
    printf("Q4. How does MIG differ from time-sliced vGPU sharing?\n");
    return 0;}
