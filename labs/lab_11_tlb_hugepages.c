/*
 * lab_11_tlb_hugepages.c
 * Topic: TLB, Large Pages, Boot Sector
 * Build: gcc -O0 -Wall -o lab_11 lab_11_tlb_hugepages.c
 * Run:   ./lab_11   (or: perf stat -e dTLB-load-misses ./lab_11)
 *
 * Author: Arun Singh | arunsingh.in@gmail.com
 * OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
static void print_section(const char *t){printf("\n========== %s ==========\n",t);}
static double now_sec(void){struct timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);return ts.tv_sec+ts.tv_nsec/1e9;}

static void demo_tlb_pressure(void){
    print_section("Phase 1: TLB Pressure — 4K vs Sequential Stride");
    size_t sz=256*1024*1024;
    char *mem=mmap(NULL,sz,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(!mem||mem==MAP_FAILED){perror("mmap");return;}
    memset(mem,1,sz);
    long ps=sysconf(_SC_PAGESIZE);
    /* Stride=page_size -> touches every page -> TLB thrash */
    double t0=now_sec();
    volatile long sum=0;
    for(size_t i=0;i<sz;i+=(size_t)ps) sum+=mem[i];
    double t_page=now_sec()-t0;
    /* Stride=64 -> sequential within pages -> TLB friendly */
    t0=now_sec();sum=0;
    for(size_t i=0;i<sz;i+=64) sum+=mem[i];
    double t_seq=now_sec()-t0;
    printf("  Page-stride (%ld B): %.4fs\n",ps,t_page);
    printf("  Cache-line stride (64B): %.4fs\n",t_seq);
    printf("  Ratio: %.1fx\n",t_page/t_seq);
    printf("\n  OBSERVE: Page-stride is slower despite touching FEWER bytes because\n");
    printf("  it accesses more unique pages, causing more TLB misses.\n");
    printf("  TIP: Run with 'perf stat -e dTLB-load-misses' to see actual TLB miss counts.\n");
    munmap(mem,sz);
}

static void demo_hugepages(void){
    print_section("Phase 2: Huge Pages");
    FILE *fp=fopen("/proc/meminfo","r");
    if(!fp)return;
    char line[256];
    while(fgets(line,sizeof(line),fp)){
        if(strstr(line,"HugePages")||strstr(line,"Hugepagesize"))
            printf("  %s",line);
    }
    fclose(fp);
    /* Try to allocate a huge page */
    size_t hp_sz=2*1024*1024;
    void *hp=mmap(NULL,hp_sz,PROT_READ|PROT_WRITE,
                  MAP_PRIVATE|MAP_ANONYMOUS|MAP_HUGETLB,-1,0);
    if(hp!=MAP_FAILED){
        memset(hp,0xAB,hp_sz);
        printf("\n  Successfully allocated 2MiB huge page at %p\n",hp);
        printf("  One TLB entry covers 2MiB instead of 4KiB = 512x better coverage.\n");
        munmap(hp,hp_sz);
    } else {
        printf("\n  Huge page allocation failed (need: echo 10 > /proc/sys/vm/nr_hugepages)\n");
        printf("  In GPU DCs, huge pages are critical for DMA buffers and RDMA registrations.\n");
    }
}

static void demo_boot_overview(void){
    print_section("Phase 3: Boot Sequence Overview");
    printf("  x86 boot sequence:\n");
    printf("  1. BIOS/UEFI: POST, find boot device\n");
    printf("  2. Bootloader (GRUB): load kernel + initramfs into memory\n");
    printf("  3. Real mode -> Protected mode -> Long mode (64-bit)\n");
    printf("  4. Kernel: setup page tables, enable paging, start scheduler\n");
    printf("  5. init/systemd (PID 1): mount filesystems, start services\n\n");
    printf("  Boot timeline from dmesg:\n");
    FILE *fp=popen("dmesg 2>/dev/null | head -20","r");
    if(fp){char l[256];while(fgets(l,sizeof(l),fp))printf("  %s",l);pclose(fp);}
    else printf("  (need root for dmesg)\n");
}

int main(void){
    printf("=== Lab 11: TLB, Large Pages, Boot Sector ===\n");
    demo_tlb_pressure();
    demo_hugepages();
    demo_boot_overview();
    printf("\n========== Quiz ==========\n");
    printf("Q1. Why does page-stride access cause more TLB misses than cache-line stride?\n");
    printf("Q2. How do 2MiB huge pages reduce TLB pressure? Calculate the coverage ratio.\n");
    printf("Q3. When the kernel boots, paging is not yet enabled. How does it access memory?\n");
    printf("Q4. Why are huge pages important for GPU DMA and RDMA in data centres?\n");
    printf("Q5. What is the role of initramfs in the boot process?\n");
    return 0;
}
