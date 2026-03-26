/*
 * lab_29_storage_fs.c
 * Topic: Storage Devices, FS Interfaces
 * Build: gcc -O0 -Wall -pthread -o lab_29 lab_29_storage_fs.c
 *
 * Author: Arun Singh | arunsingh.in@gmail.com
 * OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <fcntl.h>
static void ps(const char *t){printf("\n========== %s ==========\n",t);}
int main(void){
    printf("=== Lab 29: Storage Devices, Filesystem Interfaces ===\n");
    ps("Phase 1: Block Devices");
    FILE *fp=fopen("/proc/partitions","r");
    if(fp){char l[256];while(fgets(l,sizeof(l),fp))printf("  %s",l);fclose(fp);}
    ps("Phase 2: Filesystem Stats (statvfs)");
    struct statvfs sv;
    if(statvfs("/",&sv)==0){
        printf("  Filesystem /:\n");
        printf("    Block size:  %lu\n",sv.f_bsize);
        printf("    Total blocks:%lu (%.1f GiB)\n",sv.f_blocks,sv.f_blocks*sv.f_bsize/1073741824.0);
        printf("    Free blocks: %lu (%.1f GiB)\n",sv.f_bfree,sv.f_bfree*sv.f_bsize/1073741824.0);
        printf("    Total inodes:%lu\n",sv.f_files);
        printf("    Free inodes: %lu\n",sv.f_ffree);
    }
    ps("Phase 3: stat() — File Metadata");
    struct stat st;
    if(stat("/etc/hostname",&st)==0){
        printf("  /etc/hostname:\n");
        printf("    inode:  %lu\n",st.st_ino);
        printf("    size:   %ld bytes\n",st.st_size);
        printf("    blocks: %ld (512B blocks)\n",st.st_blocks);
        printf("    links:  %lu\n",(unsigned long)st.st_nlink);
    }
    ps("Phase 4: I/O Schedulers");
    fp=fopen("/sys/block/sda/queue/scheduler","r");
    if(!fp)fp=fopen("/sys/block/vda/queue/scheduler","r");
    if(fp){char l[256];if(fgets(l,sizeof(l),fp))printf("  I/O scheduler: %s",l);fclose(fp);}
    else printf("  (check /sys/block/*/queue/scheduler)\n");
    printf("  Modern: mq-deadline, bfq, none (NVMe often uses none).\n");

    /* WHY: NVMe uses 'none' scheduler because NVMe has its own internal queuing (NCQ,
     *      up to 65535 queues with 65535 commands each on NVMe 1.3+).  Adding a kernel
     *      I/O scheduler on top serializes requests unnecessarily.  The kernel's
     *      block-mq (multi-queue) layer maps each CPU to a hardware submission queue.
     *      With 'none', requests go directly to the NVMe queue without reordering.
     *      io_uring with IORING_SETUP_IOPOLL polls the NVMe completion queue directly,
     *      eliminating interrupt latency for the critical path.
     *
     * WHY: For sequential vs random read latency:
     *      - Sequential: the OS read-ahead prefetcher fetches pages ahead of the current
     *        read pointer, so most requests hit the page cache (zero disk I/O).
     *      - Random: each pread() on a cold file causes a page fault and disk I/O.
     *      NVMe random read latency: ~70 us (PCIe Gen4); DRAM: ~100 ns.
     *      This is why database checkpointing uses sequential I/O patterns for NVMe.
     */
    ps("Phase 5: Sequential vs Random Read Latency with pread()");
    {
        char tmpfile[] = "/tmp/lab29_ioXXXXXX";
        int fd = mkstemp(tmpfile);
        if (fd < 0) { perror("mkstemp"); goto done; }

        /* Create a 4 MiB test file */
        size_t file_size = 4 * 1024 * 1024;
        size_t page_size = (size_t)sysconf(_SC_PAGESIZE);
        char *wbuf = malloc(page_size);
        if (!wbuf) { close(fd); unlink(tmpfile); goto done; }
        memset(wbuf, 0xAB, page_size);
        for (size_t i = 0; i < file_size / page_size; i++)
            write(fd, wbuf, page_size);
        fsync(fd);

        /* Sequential reads */
        char *rbuf = malloc(page_size);
        if (!rbuf) { free(wbuf); close(fd); unlink(tmpfile); goto done; }

        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (size_t i = 0; i < file_size / page_size; i++) {
            off_t offset = (off_t)(i * page_size);
            ssize_t n = pread(fd, rbuf, page_size, offset);
            (void)n;
        }
        clock_gettime(CLOCK_MONOTONIC, &t1);
        double seq_ms = (t1.tv_sec - t0.tv_sec) * 1000.0 + (t1.tv_nsec - t0.tv_nsec) / 1e6;

        /* Random reads (access every other page to defeat prefetcher) */
        clock_gettime(CLOCK_MONOTONIC, &t0);
        size_t npages = file_size / page_size;
        for (size_t i = 0; i < npages; i++) {
            /* Stride by half the pages to maximize cache misses */
            size_t idx = (i * (npages / 2 + 1)) % npages;
            off_t offset = (off_t)(idx * page_size);
            ssize_t n = pread(fd, rbuf, page_size, offset);
            (void)n;
        }
        clock_gettime(CLOCK_MONOTONIC, &t1);
        double rnd_ms = (t1.tv_sec - t0.tv_sec) * 1000.0 + (t1.tv_nsec - t0.tv_nsec) / 1e6;

        printf("  File: 4 MiB, %zu pages of %zu B\n", npages, page_size);
        printf("  Sequential pread (all pages): %.2f ms  (%.0f us/page avg)\n",
               seq_ms, seq_ms * 1000.0 / npages);
        printf("  Strided pread   (all pages): %.2f ms  (%.0f us/page avg)\n",
               rnd_ms, rnd_ms * 1000.0 / npages);
        printf("  (On tmpfs/ramfs: both ~equal; on NVMe: random is 10-100x slower)\n");
        printf("  OBSERVE: Strided defeats prefetcher; cold NVMe: ~70 us/random IO.\n");

        free(wbuf); free(rbuf);
        close(fd); unlink(tmpfile);
        done:;
    }

    /* === EXERCISE ===
     * Try these hands-on tasks:
     * 1. Measure sequential vs random pread() latency on an ACTUAL storage device
     *    (not tmpfs).  Use O_DIRECT to bypass page cache:
     *      fd = open("/mnt/nvme/testfile", O_RDONLY | O_DIRECT);
     *    Compare: page cache warm (second run) vs cold (first run, or after
     *      echo 3 > /proc/sys/vm/drop_caches).
     * 2. Implement async I/O with io_uring: submit 32 pread SQEs in one batch,
     *    then call io_uring_enter() once.  Compare ops/sec vs blocking pread().
     *    For NVMe with IORING_SETUP_IOPOLL: poll instead of interrupt, ~20% faster.
     * 3. Modern (NVMe queues + io_uring for GPU servers): AI training checkpoints
     *    write model weights to NVMe SSDs.  With GDS (GPU Direct Storage, cuFile API),
     *    the GPU DMA engine writes directly to the NVMe without CPU involvement.
     *    Check: cat /sys/block/nvme0n1/queue/nr_requests  (NVMe queue depth)
     *    and compare to HDD scheduler: cat /sys/block/sda/queue/scheduler
     *
     * OBSERVE: Sequential access leverages the kernel's read-ahead: the first pread()
     *          triggers read-ahead of 64+ pages, so subsequent reads hit page cache.
     *          Random access defeats read-ahead: every access is a new page cache miss.
     * WHY:     NVMe 'none' scheduler is correct because NVMe has internal 64K-depth
     *          queues per namespace.  Adding mq-deadline on top adds latency for the
     *          common case (SSD with no seek time to optimize).
     */
    printf("\n========== Hands-On Exercise ==========\n");
    printf("1. Measure pread() latency with O_DIRECT on NVMe vs tmpfs;\n");
    printf("   compare warm page cache vs cold (after drop_caches).\n");
    printf("2. Implement io_uring batched pread: 32 SQEs per io_uring_enter();\n");
    printf("   compare ops/sec vs blocking pread.\n");
    printf("3. Modern (NVMe + GPU Direct Storage): check /sys/block/nvme*/queue/nr_requests;\n");
    printf("   explain why NVMe uses 'none' scheduler and GDS bypasses CPU for checkpoint I/O.\n");

    printf("\n========== Quiz ==========\n");
    printf("Q1. What is an inode and what metadata does it store?\n");
    printf("Q2. Why does NVMe often use the 'none' I/O scheduler?\n");
    printf("Q3. What is the difference between f_bfree and f_bavail in statvfs?\n");
    printf("Q4. How does the VFS layer abstract different filesystems?\n");
    printf("Q5. What is the kernel read-ahead mechanism and how does it make sequential\n");
    printf("    reads fast even for cold files?\n");
    printf("Q6. How does io_uring IORING_SETUP_IOPOLL eliminate interrupt overhead\n");
    printf("    for NVMe I/O completion, and when is polling better than interrupts?\n");
    printf("Q7. What is GPU Direct Storage (GDS/cuFile), and why does it matter for\n");
    printf("    AI training checkpoint performance on NVMe-attached GPU servers?\n");
    return 0;
}
