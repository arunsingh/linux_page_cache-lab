/*
 * lab_31_fs_ops.c
 * Topic: Filesystem Operations
 * Build: gcc -O0 -Wall -pthread -o lab_31 lab_31_fs_ops.c
 *
 * Author: Arun Singh | arunsingh.in@gmail.com
 * OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
*/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
static void ps(const char *t){printf("\n========== %s ==========\n",t);}
static double now(void){struct timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);return ts.tv_sec+ts.tv_nsec/1e9;}
int main(void){
    printf("=== Lab 31: File System Operations ===\n");
    ps("Phase 1: open/write/fsync Path");
    double t0=now();
    int fd=open("/tmp/lab31_test.dat",O_CREAT|O_WRONLY|O_TRUNC,0644);
    char buf[4096];memset(buf,'A',sizeof(buf));
    for(int i=0;i<1000;i++)write(fd,buf,sizeof(buf));
    double t_write=now()-t0;
    t0=now();fsync(fd);double t_fsync=now()-t0;
    close(fd);
    printf("  Write 4MiB: %.3f ms (to page cache)\n",t_write*1000);
    printf("  fsync:       %.3f ms (flush to disk)\n",t_fsync*1000);
    printf("  OBSERVE: write() returns immediately (page cache). fsync() waits for disk.\n");
    ps("Phase 2: O_DIRECT Bypass");
    printf("  O_DIRECT bypasses page cache — DMA directly to user buffer.\n");
    printf("  Used by databases (PostgreSQL, MySQL) for their own caching.\n");
    printf("  GPU DCs: GPU Direct Storage uses similar bypass for GPU memory.\n");
    ps("Phase 3: File Descriptor Table");
    printf("  Each process has an fd table -> file description (open file) -> inode.\n");
    printf("  After fork(): child gets copy of fd table, shares open file descriptions.\n");
    printf("  After dup2(): two fds point to same open file description.\n");
    unlink("/tmp/lab31_test.dat");

    /* WHY: The page cache (write-back cache) is the kernel's central I/O buffer.
     *      Dirty pages accumulate in memory and are written to disk by:
     *      - The writeback daemon (pdflush/kworker) when dirty ratio exceeds threshold
     *      - Explicitly via fsync() / fdatasync() / sync()
     *      - When the page is evicted from page cache (memory pressure)
     *      /proc/sys/vm/dirty_ratio: max % of RAM that can be dirty before writeback
     *      /proc/sys/vm/dirty_background_ratio: % at which background writeback starts
     *
     * WHY: DAX (Direct Access) for persistent memory (PMEM, Intel Optane):
     *      DAX filesystems (ext4 with -o dax, xfs with -o dax) bypass the page cache
     *      entirely.  mmap() on a DAX file maps directly to PMEM physical addresses.
     *      Reads/writes go directly to PMEM without copying through DRAM page cache.
     *      This is critical for PMEM's ~300 ns latency (vs DRAM ~100 ns, NVMe ~70 us).
     *      CLFLUSHOPT + SFENCE ensure PMEM stores are persistent after a power failure.
     *      Linux 5.1+: FS-DAX with hole punching; Linux 6.x: DAX with ZONE_DEVICE pages.
     */
    ps("Phase 4: File Copy Bandwidth + Page Cache Behavior");
    {
        /* Measure file copy bandwidth: read -> write */
        char src[] = "/tmp/lab31_srcXXXXXX";
        char dst[] = "/tmp/lab31_dstXXXXXX";
        int sfd = mkstemp(src), dfd = mkstemp(dst);
        if (sfd < 0 || dfd < 0) { goto cleanup31; }

        /* Write 8 MiB source file */
        char fbuf[65536];
        memset(fbuf, 0xCC, sizeof(fbuf));
        size_t total = 8 * 1024 * 1024;
        for (size_t i = 0; i < total / sizeof(fbuf); i++)
            write(sfd, fbuf, sizeof(fbuf));
        fsync(sfd); lseek(sfd, 0, SEEK_SET);

        /* Copy and measure bandwidth */
        double tc = now();
        ssize_t nr;
        while ((nr = read(sfd, fbuf, sizeof(fbuf))) > 0)
            write(dfd, fbuf, (size_t)nr);
        fsync(dfd);
        double copy_time = now() - tc;

        struct stat st;
        fstat(sfd, &st);
        double bw_mbps = (st.st_size / 1048576.0) / copy_time;
        printf("  File copy 8 MiB: %.3f ms  bandwidth: %.0f MiB/s\n",
               copy_time * 1000.0, bw_mbps);
        printf("  (Page cache -> page cache copy; disk I/O only on fsync)\n");
        printf("  With O_DIRECT: bypasses page cache; bandwidth limited by NVMe/disk.\n");
        printf("  With DAX + PMEM: mmap -> mmap copy at ~DRAM bandwidth (~30-50 GB/s).\n");

        cleanup31:
        if (sfd >= 0) { close(sfd); unlink(src); }
        if (dfd >= 0) { close(dfd); unlink(dst); }
    }

    /* === EXERCISE ===
     * Try these hands-on tasks:
     * 1. Benchmark write() without fsync vs write() with fsync() for 1000 4K writes.
     *    The without-fsync version writes to page cache (~microseconds per write).
     *    The with-fsync version waits for disk (~milliseconds per write on HDD,
     *    ~microseconds on NVMe with NVMe completion latency).
     * 2. Implement an atomic file update pattern (used by databases):
     *    a) Write new content to a temp file in the same directory
     *    b) fsync() the temp file
     *    c) rename() temp -> final (atomic on POSIX)
     *    d) fsync() the directory to make the rename durable
     *    This ensures either old or new content is visible after crash, never partial.
     * 3. Modern (DAX for persistent memory): if you have access to PMEM or can use
     *    a ramdisk with -o dax:
     *      mount -t ext4 -o dax /dev/pmem0 /mnt/pmem
     *    Compare mmap bandwidth with vs without DAX:
     *    DAX: PMEM bandwidth (~30 GB/s); non-DAX: page cache copy overhead.
     *    Check: cat /sys/block/pmem0/queue/rotational (should be 0 for PMEM)
     *
     * OBSERVE: write() to a regular file returns after writing to the page cache.
     *          The dirty page is NOT on disk yet -- a power failure loses the data.
     *          fdatasync() is faster than fsync() when only data (not metadata) needs
     *          persistence: it skips flushing timestamps and other non-critical inode fields.
     * WHY:     /proc/sys/vm/dirty_ratio (default 20%): if 20% of RAM is dirty, ALL
     *          write() calls block until writeback drains to dirty_background_ratio (10%).
     *          This "dirty throttling" prevents runaway writeback and OOM from dirty pages.
     */
    printf("\n========== Hands-On Exercise ==========\n");
    printf("1. Benchmark 1000 x 4K write() with vs without fsync();\n");
    printf("   observe page cache (fast) vs disk flush (slow) difference.\n");
    printf("2. Implement atomic file update: temp write -> fsync -> rename -> fsync dir.\n");
    printf("3. Modern (DAX + PMEM): mount ext4 with -o dax; measure mmap bandwidth\n");
    printf("   vs non-DAX; observe page cache is bypassed for PMEM-backed files.\n");

    printf("\n========== Quiz ==========\n");
    printf("Q1. Why is write() so much faster than fsync()? Where does data go?\n");
    printf("Q2. What data can be lost if the system crashes before fsync()?\n");
    printf("Q3. What is the difference between fsync() and fdatasync()?\n");
    printf("Q4. Why would a database use O_DIRECT?\n");
    printf("Q5. What is the dirty_ratio kernel parameter and what happens when it is exceeded?\n");
    printf("Q6. What is DAX (Direct Access) for persistent memory?  How does it differ from\n");
    printf("    normal file I/O, and which filesystem feature must be enabled?\n");
    printf("Q7. In a distributed ML training cluster, why must checkpoint writes use fsync()\n");
    printf("    or O_SYNC rather than relying on page cache write-back?\n");
    return 0;
}
