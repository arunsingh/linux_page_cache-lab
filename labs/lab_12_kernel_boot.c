/*
 * lab_12_kernel_boot.c
 * Topic: Loading the Kernel, Initializing the Page Table
 * Parses dmesg for boot timeline, shows kernel command line, early memory setup.
 * Build: gcc -O0 -Wall -o lab_12 lab_12_kernel_boot.c
 *
 * Author: Arun Singh | arunsingh.in@gmail.com
 * OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
*/
#include <stdio.h>
#include <string.h>
#include <unistd.h>
static void print_section(const char *t){printf("\n========== %s ==========\n",t);}

static void demo_cmdline(void){
    print_section("Phase 1: Kernel Command Line");
    FILE *fp=fopen("/proc/cmdline","r");
    if(fp){char l[1024];if(fgets(l,sizeof(l),fp))printf("  %s\n",l);fclose(fp);}
    printf("  Key parameters: root= (root fs), console= (serial), hugepagesz=, isolcpus=\n");
    printf("  GPU DC kernels often have: iommu=pt default_hugepagesz=1G isolcpus=1-N\n");
}

static void demo_boot_memory(void){
    print_section("Phase 2: Early Memory Setup (dmesg)");
    const char *patterns[]={"Memory:","BIOS-e820:","page tables","kernel","Zone",NULL};
    FILE *fp=popen("dmesg 2>/dev/null","r");
    if(!fp){printf("  (need root)\n");return;}
    char line[512];int shown=0;
    while(fgets(line,sizeof(line),fp)&&shown<25){
        for(int i=0;patterns[i];i++){
            if(strstr(line,patterns[i])){printf("  %s",line);shown++;break;}
        }
    }
    pclose(fp);
    printf("\n  OBSERVE: BIOS-e820 provides the physical memory map to the kernel.\n");
    printf("  The kernel builds its initial page tables to map itself and the direct-map region.\n");
}

static void demo_kernel_version(void){
    print_section("Phase 3: Kernel Version & Config");
    FILE *fp=fopen("/proc/version","r");
    if(fp){char l[512];if(fgets(l,sizeof(l),fp))printf("  %s",l);fclose(fp);}
    printf("\n  Current kernel config highlights:\n");
    fp=popen("zcat /proc/config.gz 2>/dev/null | grep -E 'HUGETLB|TRANSPARENT_HUGE|NUMA|PREEMPT|HZ=' | head -10","r");
    if(fp){char l[256];while(fgets(l,sizeof(l),fp))printf("  %s",l);pclose(fp);}
    else printf("  (/proc/config.gz not available — check /boot/config-*)\n");
}

int main(void){
    printf("=== Lab 12: Loading the Kernel, Initializing Page Tables ===\n");
    demo_cmdline();
    demo_boot_memory();
    demo_kernel_version();
    printf("\n========== Quiz ==========\n");
    printf("Q1. What is the e820 memory map and who provides it?\n");
    printf("Q2. What is the kernel direct-map region and why does Linux map all physical RAM there?\n");
    printf("Q3. What does 'iommu=pt' on the kernel command line do for GPU servers?\n");
    printf("Q4. Why does the kernel need early page tables before the memory allocator is ready?\n");
    return 0;
}
