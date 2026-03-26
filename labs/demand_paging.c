/*
 * demand_paging.c
 *
 * Extended Linux demand paging lab.
 *
 * This program is intentionally interactive so a candidate can correlate:
 *   - virtual memory reservation (mmap)
 *   - demand paging on first touch
 *   - read-only first touch via the shared zero page
 *   - write faults that allocate anonymous pages
 *   - page release via munmap
 *
 * Suggested workflow:
 *   Terminal 1: ./demand_paging
 *   Terminal 2: ./monitor.sh <pid>
 *
 * Build:
 *   gcc -O0 -Wall -pthread -o demand_paging demand_paging.c
 *
 * Author: Arun Singh | arunsingh.in@gmail.com
 * OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
*/

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define TOTAL_MIB   100
#define CHUNK_MIB   10
#define MIB         (1024UL * 1024UL)
#define TOTAL_BYTES (TOTAL_MIB * MIB)
#define CHUNK_BYTES (CHUNK_MIB * MIB)

static void wait_for_enter(const char *msg)
{
    int c;
    printf("%s", msg);
    fflush(stdout);
    while ((c = getchar()) != '\n' && c != EOF) {
    }
}

static long page_size(void)
{
    long ps = sysconf(_SC_PAGESIZE);
    if (ps <= 0) {
        perror("sysconf(_SC_PAGESIZE)");
        exit(1);
    }
    return ps;
}

int main(void)
{
    long ps = page_size();
    size_t pages_per_chunk = CHUNK_BYTES / (size_t)ps;
    size_t total_pages = TOTAL_BYTES / (size_t)ps;

    printf("=== Linux Demand Paging Experiment (Extended) ===\n");
    printf("PID                : %d\n", getpid());
    printf("Page size          : %ld bytes\n", ps);
    printf("Virtual reserve    : %d MiB\n", TOTAL_MIB);
    printf("Touch chunk        : %d MiB\n", CHUNK_MIB);
    printf("Pages per chunk    : %zu\n", pages_per_chunk);
    printf("Total pages        : %zu\n", total_pages);
    printf("\nRun the following in another terminal to monitor memory usage:\n");
    printf("  ./monitor.sh %d\n\n", getpid());
    printf("Observe VmSize, VmRSS, minor faults, and major faults.\n\n");

    wait_for_enter("[1] Press Enter to reserve 100 MiB with mmap()...\n");

    uint8_t *mem = mmap(NULL,
                        TOTAL_BYTES,
                        PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS,
                        -1,
                        0);
    if (mem == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    printf("    Reserved virtual range at %p\n", (void *)mem);
    printf("    VmSize should jump now. VmRSS should barely move.\n");
    printf("    No real anonymous pages are committed until first touch.\n\n");

    wait_for_enter("[2] Press Enter to READ one byte from the first page of each 10 MiB chunk...\n");

    volatile uint64_t checksum = 0;
    for (size_t offset = 0; offset < TOTAL_BYTES; offset += CHUNK_BYTES) {
        checksum += mem[offset];
        printf("    Read first byte at offset %7zu MiB\n", offset / MIB);
        fflush(stdout);
        sleep(1);
    }

    printf("    Read phase complete. checksum=%llu\n", (unsigned long long)checksum);
    printf("    Expect some minor faults, but VmRSS may remain tiny because anonymous\n");
    printf("    reads can map the shared zero page instead of allocating private pages.\n\n");

    wait_for_enter("[3] Press Enter to WRITE 10 MiB per second and force real page allocation...\n");

    int chunks = TOTAL_MIB / CHUNK_MIB;
    for (int i = 0; i < chunks; i++) {
        size_t offset = (size_t)i * CHUNK_BYTES;
        memset(mem + offset, (uint8_t)(i + 1), CHUNK_BYTES);
        printf("    Written: %3d / %d MiB\n", (i + 1) * CHUNK_MIB, TOTAL_MIB);
        printf("    Expect about %zu new minor faults for this chunk and VmRSS growth.\n",
               pages_per_chunk);
        fflush(stdout);
        sleep(1);
    }

    wait_for_enter("[4] Press Enter to scan all pages again (warm anonymous pages)...\n");

    checksum = 0;
    for (size_t offset = 0; offset < TOTAL_BYTES; offset += (size_t)ps) {
        checksum += mem[offset];
    }

    printf("    Warm scan complete. checksum=%llu\n", (unsigned long long)checksum);
    printf("    Expect low additional faults because pages are already mapped.\n\n");

    wait_for_enter("[5] Press Enter to release memory with munmap()...\n");

    if (munmap(mem, TOTAL_BYTES) != 0) {
        perror("munmap");
        return 1;
    }

    printf("    Memory released. VmSize and VmRSS should drop back near baseline.\n");
    printf("\nQuick quiz for the candidate:\n");
    printf("  Q1. Why did VmSize jump immediately after mmap() but VmRSS did not?\n");
    printf("  Q2. Why can the READ phase show minor faults without large RSS growth?\n");
    printf("  Q3. Why are major faults usually zero in this anonymous memory test?\n");
    printf("  Q4. Why does the second full scan cause far fewer faults than the write phase?\n");
    return 0;
}
