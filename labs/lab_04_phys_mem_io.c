/*
 * lab_04_phys_mem_io.c
 *
 * Topic: Physical Memory Map, I/O, Segmentation
 *
 * Build: gcc -O0 -Wall -o lab_04 lab_04_phys_mem_io.c
 *
 * Author: Arun Singh | arunsingh.in@gmail.com
 * OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

static void print_section(const char *t) { printf("\n========== %s ==========\n", t); }

static void demo_iomem(void) {
    print_section("Phase 1: Physical Memory Map (/proc/iomem)");
    printf("  /proc/iomem shows how physical address space is divided:\n");
    printf("  RAM, reserved BIOS regions, PCI MMIO bars, GPU framebuffer, etc.\n\n");

    FILE *fp = fopen("/proc/iomem", "r");
    if (!fp) { printf("  (need root to read /proc/iomem — run: sudo cat /proc/iomem)\n"); return; }
    char line[256]; int n = 0;
    while (fgets(line, sizeof(line), fp) && n++ < 30) printf("  %s", line);
    fclose(fp);
    printf("\n  OBSERVE: 'System RAM' = usable memory. PCI regions = device MMIO.\n");
    printf("  In GPU servers, you'll see large MMIO regions for each GPU's BAR space.\n");
}

static void demo_ioports(void) {
    print_section("Phase 2: I/O Ports (/proc/ioports)");
    FILE *fp = fopen("/proc/ioports", "r");
    if (!fp) { printf("  (need root — run: sudo cat /proc/ioports)\n"); return; }
    char line[256]; int n = 0;
    while (fgets(line, sizeof(line), fp) && n++ < 15) printf("  %s", line);
    fclose(fp);
    printf("\n  OBSERVE: Legacy x86 uses port I/O (in/out instructions).\n");
    printf("  Modern devices use MMIO instead — memory-mapped into physical address space.\n");
}

static void demo_segments(void) {
    print_section("Phase 3: Segmentation (historical context)");
    printf("  x86 segmentation: code segment (CS), data segment (DS), stack segment (SS).\n");
    printf("  In 64-bit Linux, segmentation is essentially disabled:\n");
    printf("    CS, DS, SS all have base=0, limit=max (flat model).\n");
    printf("    FS/GS are used for thread-local storage (TLS) and percpu data.\n\n");

#if defined(__x86_64__)
    unsigned short cs, ds, ss, fs, gs;
    __asm__ volatile("mov %%cs, %0" : "=r"(cs));
    __asm__ volatile("mov %%ds, %0" : "=r"(ds));
    __asm__ volatile("mov %%ss, %0" : "=r"(ss));
    __asm__ volatile("mov %%fs, %0" : "=r"(fs));
    __asm__ volatile("mov %%gs, %0" : "=r"(gs));
    printf("  CS=0x%04x DS=0x%04x SS=0x%04x FS=0x%04x GS=0x%04x\n", cs, ds, ss, fs, gs);
    printf("  FS is non-zero = used for glibc TLS (thread-local storage).\n");
#endif
}

int main(void) {
    printf("=== Lab 04: Physical Memory Map, I/O, Segmentation ===\n");
    demo_iomem();
    demo_ioports();
    demo_segments();
    printf("\n========== Quiz ==========\n");
    printf("Q1. What is the difference between port I/O and memory-mapped I/O?\n");
    printf("Q2. Why do GPU servers show large MMIO regions in /proc/iomem?\n");
    printf("Q3. Why is x86 segmentation mostly irrelevant in 64-bit Linux?\n");
    printf("Q4. What is FS register used for in modern Linux?\n");
    return 0;
}
