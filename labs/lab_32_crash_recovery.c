/*
 * lab_32_crash_recovery.c
 * Topic: Crash Recovery and Logging
 * Build: gcc -O0 -Wall -pthread -o lab_32 lab_32_crash_recovery.c
 *
 * Author: Arun Singh | arunsingh.in@gmail.com
 * OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
static void ps(const char *t){printf("\n========== %s ==========\n",t);}
int main(void){
    printf("=== Lab 32: Crash Recovery and Logging ===\n");
    ps("Phase 1: Why Journals Exist");
    printf("  Problem: updating a file involves multiple disk writes:\n");
    printf("    1. Update data blocks\n");
    printf("    2. Update inode (size, timestamps)\n");
    printf("    3. Update free block bitmap\n");
    printf("  Crash between steps -> inconsistent filesystem.\n\n");
    printf("  Solution: Write-Ahead Logging (journaling):\n");
    printf("    1. Write all changes to journal first\n");
    printf("    2. Write commit record\n");
    printf("    3. Apply changes to actual locations\n");
    printf("    4. Mark journal entry as done\n");
    printf("  On crash recovery: replay committed journal entries.\n");
    ps("Phase 2: fsync Ordering Demo");
    /* Create file, write, rename atomically */
    int fd=open("/tmp/lab32_new.dat",O_CREAT|O_WRONLY|O_TRUNC,0644);
    write(fd,"important data",14);
    fsync(fd);close(fd);
    rename("/tmp/lab32_new.dat","/tmp/lab32_final.dat");
    /* fsync the directory to ensure rename is durable */
    fd=open("/tmp",O_RDONLY);
    fsync(fd);close(fd);
    printf("  Safe atomic file update: write new -> fsync -> rename -> fsync dir\n");
    unlink("/tmp/lab32_final.dat");
    ps("Phase 3: Journal Modes");
    printf("  ext4 journal modes:\n");
    printf("    journal:  data + metadata journaled (safest, slowest)\n");
    printf("    ordered:  metadata journaled, data written before metadata (default)\n");
    printf("    writeback: metadata journaled, data can be stale (fastest)\n");
    /* WHY: ZFS uses a different approach from ext4 journal:
     *      - ZFS uses a Zettabyte Intent Log (ZIL) for synchronous writes.
     *      - All writes go to a copy-on-write (COW) transaction model.
     *      - No "journal" in the ext4 sense; instead, ZFS uses Transaction Groups (TXG).
     *      - Each TXG is committed atomically every 5 seconds by default.
     *      - On crash: incomplete TXG is discarded; previous TXG is complete.
     *      - ZFS also does checksumming of all data/metadata blocks (unlike ext4).
     *      - "Silent corruption" (bit rot) is detected and corrected by ZFS scrub.
     *
     * WHY: O_SYNC vs fsync():
     *      O_SYNC: every write() call blocks until data AND metadata are on disk.
     *      O_DSYNC: every write() blocks until data is on disk (metadata not required).
     *      fsync(): after a batch of writes, explicitly flush all buffered data.
     *      For databases: O_DSYNC or fdatasync() is typically faster than O_SYNC.
     *      PostgreSQL: uses fdatasync() by default; configurable in postgresql.conf.
     */
    ps("Phase 4: Simulating Crash Recovery Scenario");
    {
        /* Demonstrate the safe atomic write pattern */
        char tmpdata[] = "/tmp/lab32_new_XXXXXX";
        int fd = mkstemp(tmpdata);
        if (fd >= 0) {
            /* Step 1: write new data to temp file */
            const char *new_content = "new important data v2\n";
            write(fd, new_content, strlen(new_content));

            /* Step 2: fsync temp file -- data persisted */
            fsync(fd);
            close(fd);
            printf("  Step 1: wrote to temp file %s\n", tmpdata);
            printf("  Step 2: fsync'd temp file (data durable on disk)\n");

            /* Step 3: atomic rename -- either old or new is visible, never partial */
            char finalname[] = "/tmp/lab32_final_v2.dat";
            rename(tmpdata, finalname);
            printf("  Step 3: rename() -- atomic POSIX operation\n");
            printf("          After crash: either old file OR new file, never half-written\n");

            /* Step 4: fsync directory -- make rename durable */
            int dfd = open("/tmp", O_RDONLY);
            if (dfd >= 0) { fsync(dfd); close(dfd); }
            printf("  Step 4: fsync'd /tmp directory (rename durable across crash)\n");
            printf("  WHY: Without dir fsync, the rename might not survive a reboot.\n");

            unlink(finalname);
        }
    }

    printf("\n  ZFS vs ext4 journal comparison:\n");
    printf("    ext4 journal: WAL, metadata-first, replay on mount after crash\n");
    printf("    ZFS ZIL: intent log for synchronous writes; TXG for async commits\n");
    printf("    ZFS advantage: checksumming detects silent corruption (ext4 does not)\n");
    printf("    ZFS disadvantage: higher write amplification on random writes\n");

    /* === EXERCISE ===
     * Try these hands-on tasks:
     * 1. Simulate a "crash" mid-write:
     *    a) Write 4 MiB to a file WITHOUT fsync.
     *    b) Use kill(getpid(), SIGKILL) to terminate the process.
     *    c) Observe: the file may be truncated or contain partial data on reread.
     *    d) Repeat WITH fsync before kill: file should contain complete data.
     *    Note: on ext4 with data=ordered, data is written before metadata, so the
     *    inode size update may be lost even if data blocks are on disk.
     * 2. Measure fsync latency for different file sizes:
     *    Write 4K, 64K, 1MB, 4MB and measure fsync time for each.
     *    On NVMe: fsync latency is mostly constant (flush command latency ~100us).
     *    On HDD: fsync latency grows with file size (rotational seek + settle time).
     * 3. Modern (ZFS vs ext4 journal): if ZFS is available (zfs-linux package):
     *      zpool create testpool /dev/loop0
     *      zfs create testpool/data
     *    Write 100K small files to ZFS vs ext4 and measure throughput.
     *    Check: zpool status testpool (shows checksum errors if any)
     *    Use: zdb -l /dev/loop0 to inspect ZFS labels and uberblocks.
     *
     * OBSERVE: fsync() on NVMe takes ~100-500 us (NVMe flush command + controller ack).
     *          On HDD: ~5-20 ms (rotational latency + seek).
     *          This is why databases on HDD traditionally have much lower TPS than on NVMe.
     * WHY:     The rename() + fsync(dir) pattern is the POSIX-correct way to atomically
     *          replace a file.  Without fsync(dir), the directory entry (rename result)
     *          may not be durable.  Some filesystems (ext4 with dirsync mount option)
     *          do this automatically; others require explicit fsync on the directory.
     */
    printf("\n========== Hands-On Exercise ==========\n");
    printf("1. Simulate crash mid-write with SIGKILL; observe partial data without fsync;\n");
    printf("   repeat with fsync and verify complete data survives.\n");
    printf("2. Measure fsync latency for 4K/64K/1M/4M files on NVMe vs HDD;\n");
    printf("   observe NVMe is nearly constant (flush cmd latency).\n");
    printf("3. Modern (ZFS): create ZFS pool; compare 100K file write throughput vs ext4;\n");
    printf("   use 'zpool status' to verify checksums; run 'zpool scrub' to verify data.\n");

    printf("\n========== Quiz ==========\n");
    printf("Q1. What is write-ahead logging and why is it needed?\n");
    printf("Q2. What can go wrong with rename() without fsync on the directory?\n");
    printf("Q3. What is the difference between ext4 ordered and writeback modes?\n");
    printf("Q4. How does journaling relate to database WAL (e.g., PostgreSQL)?\n");
    printf("Q5. What is the difference between O_SYNC, O_DSYNC, and fsync()?  When\n");
    printf("    would you use each for a write-heavy database workload?\n");
    printf("Q6. How does ZFS achieve crash consistency without a traditional journal?\n");
    printf("    What is a Transaction Group (TXG) and how does it compare to ext4 commit?\n");
    printf("Q7. Why must the rename() + fsync(dir) pattern be used for atomic file\n");
    printf("    replacement?  What failure scenario does each step prevent?\n");
    return 0;
}
