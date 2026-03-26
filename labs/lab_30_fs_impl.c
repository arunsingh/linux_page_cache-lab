/*
 * lab_30_fs_impl.c
 * Topic: Filesystem Implementation
 * Build: gcc -O0 -Wall -pthread -o lab_30 lab_30_fs_impl.c
 *
 * Author: Arun Singh | arunsingh.in@gmail.com
 * OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
static void ps(const char *t){printf("\n========== %s ==========\n",t);}
int main(void){
    printf("=== Lab 30: File System Implementation ===\n");
    ps("Phase 1: VFS Layer");
    printf("  Linux VFS provides a unified interface: superblock, inode, dentry, file.\n");
    printf("  Every filesystem (ext4, xfs, btrfs, nfs) implements these operations.\n\n");
    printf("  Key structures:\n");
    printf("    super_block: filesystem-wide info (block size, mount options)\n");
    printf("    inode: per-file metadata (permissions, size, block pointers)\n");
    printf("    dentry: directory entry cache (name -> inode mapping)\n");
    printf("    file: per-open-file state (position, flags)\n");
    ps("Phase 2: Mount Info");
    FILE *fp=fopen("/proc/mounts","r");
    if(fp){char l[256];int n=0;while(fgets(l,sizeof(l),fp)&&n++<10)printf("  %s",l);fclose(fp);}
    ps("Phase 3: Dentry Cache Stats");
    fp=fopen("/proc/sys/fs/dentry-state","r");
    if(fp){char l[256];if(fgets(l,sizeof(l),fp))printf("  dentry-state: %s",l);fclose(fp);
        printf("  Format: nr_dentry nr_unused age_limit want_pages dummy dummy\n");}
    /* WHY: inode, dentry, superblock in VFS:
     *      - superblock: per-filesystem metadata (block size, journal state, root inode)
     *      - inode: per-file metadata (size, permissions, timestamps, data block pointers)
     *        NOT the filename -- inodes have no name.
     *      - dentry: name -> inode mapping, cached in dcache for fast path resolution
     *        Two hardlinks = two dentries, same inode (same st_ino from stat())
     *      - file: per-open-fd state (f_pos, f_flags, pointer to inode)
     *
     * WHY: XFS vs ext4 vs btrfs journal comparison:
     *      ext4 journaling modes:
     *        - ordered: journal metadata only; data written before metadata commit (default)
     *        - writeback: journal metadata only; data may be written after metadata
     *        - journal: journal everything (safest, slowest; 2x write amplification)
     *      XFS: uses a write-ahead log (WAL) for metadata; data journaling optional
     *      btrfs: copy-on-write (COW) filesystem; no journal needed for data integrity
     *             (new data written to new blocks; old blocks freed after transaction commit)
     *      On NVMe: ext4 ordered or XFS for performance; btrfs for snapshots/dedup.
     */
    ps("Phase 4: Inode Information from stat()");
    {
        /* Create a temp file and examine its inode */
        char tmpfile[] = "/tmp/lab30_iXXXXXX";
        int fd = mkstemp(tmpfile);
        if (fd >= 0) {
            write(fd, "hello\n", 6);
            close(fd);

            struct stat st;
            if (stat(tmpfile, &st) == 0) {
                printf("  Created: %s\n", tmpfile);
                printf("  inode number:    %lu\n", (unsigned long)st.st_ino);
                printf("  size:            %ld bytes\n", (long)st.st_size);
                printf("  disk blocks:     %ld (512B units)\n", (long)st.st_blocks);
                printf("  hard link count: %lu\n", (unsigned long)st.st_nlink);
                printf("  permissions:     0%o\n", st.st_mode & 07777);
                printf("  device:          %lu (major=%lu minor=%lu)\n",
                       (unsigned long)st.st_dev,
                       (unsigned long)((st.st_dev >> 8) & 0xff),
                       (unsigned long)(st.st_dev & 0xff));
            }

            /* Create a hard link -- same inode, different dentry */
            char link_name[] = "/tmp/lab30_linkXXXXXX";
            /* Use the same prefix for the link */
            snprintf(link_name, sizeof(link_name), "%s_link", tmpfile);
            if (link(tmpfile, link_name) == 0) {
                struct stat st2;
                stat(link_name, &st2);
                printf("\n  Hard link: %s\n", link_name);
                printf("  Same inode? %s (orig=%lu link=%lu)\n",
                       st.st_ino == st2.st_ino ? "YES" : "NO",
                       (unsigned long)st.st_ino, (unsigned long)st2.st_ino);
                printf("  Link count on both: %lu (two dentries, same inode)\n",
                       (unsigned long)st2.st_nlink);
                unlink(link_name);
            }
            unlink(tmpfile);
        }
    }

    ps("Phase 5: Filesystem Journal Modes");
    printf("  ext4 journaling modes:\n");
    printf("    ordered:  journal metadata only; data written BEFORE metadata commit (default)\n");
    printf("    writeback: journal metadata only; data may be written AFTER (faster, less safe)\n");
    printf("    journal:  journal data AND metadata (safest, ~2x write amplification)\n\n");
    printf("  XFS: write-ahead log (WAL) for metadata; data=ordered by default\n");
    printf("  btrfs: copy-on-write (no traditional journal; transactions are atomic by design)\n\n");

    /* Check current mount options for / */
    FILE *fp2 = fopen("/proc/mounts", "r");
    if (fp2) {
        char line[512];
        while (fgets(line, sizeof(line), fp2)) {
            /* Look for root filesystem mount */
            if (strncmp(line, "none", 4) != 0 && strstr(line, " / ")) {
                printf("  Root filesystem mount options: %s", line);
                /* Extract journal mode from options */
                if (strstr(line, "data=journal")) printf("  Journal mode: data=journal\n");
                else if (strstr(line, "data=writeback")) printf("  Journal mode: data=writeback\n");
                else if (strstr(line, "data=ordered")) printf("  Journal mode: data=ordered\n");
                break;
            }
        }
        fclose(fp2);
    }

    /* === EXERCISE ===
     * Try these hands-on tasks:
     * 1. Create a file, stat() it to get the inode number.  Then create a hard link
     *    and a symlink to the same file.  stat() all three and compare:
     *    - Hard link: same st_ino (same inode), st_nlink incremented
     *    - Symlink: different st_ino (symlink has its own inode), lstat() shows type=S_IFLNK
     * 2. Measure dentry cache hit rate: time a loop calling stat("/bin/ls") 1M times.
     *    The first call: path resolution (dcache miss + inode lookup).
     *    Subsequent calls: dcache hit (just atomic pointer dereference).
     *    Compare: 1M cached stats should take ~100ms; uncached first call ~10-100x slower.
     * 3. Modern (XFS vs ext4 vs btrfs): on a test filesystem, create 100K small files
     *    and measure:
     *      time for n in $(seq 1 100000); do touch /mnt/test/$n; done
     *    Compare ext4 (data=ordered), XFS, and btrfs.  ext4 is typically fastest for
     *    metadata-heavy workloads; btrfs is slowest due to COW metadata overhead.
     *
     * OBSERVE: The inode number is filesystem-local (unique within one filesystem,
     *          not globally).  This is why you cannot hardlink across filesystems:
     *          hardlink requires same inode, but inodes are per-filesystem.
     *          Symlinks work across filesystems because they store a PATH, not an inode.
     * WHY:     The dentry cache (dcache) stores name->inode mappings in a hash table.
     *          On a system with 100K files, the dcache typically holds the entire
     *          namespace in memory.  Path resolution is O(depth) dcache lookups.
     *          The rcu_read_lock() in d_lookup() allows concurrent readers without locks.
     */
    printf("\n========== Hands-On Exercise ==========\n");
    printf("1. Create file, hardlink, symlink; compare st_ino and st_nlink with stat().\n");
    printf("2. Time 1M stat('/bin/ls') calls; observe dcache hit rate (sub-microsecond).\n");
    printf("3. Modern (ext4 vs XFS vs btrfs): create 100K small files; compare\n");
    printf("   metadata performance and journal mode impact on crash recovery.\n");

    printf("\n========== Quiz ==========\n");
    printf("Q1. What is the difference between an inode and a dentry?\n");
    printf("Q2. Can two dentries point to the same inode? When does this happen?\n");
    printf("Q3. What does the dentry cache do and why is it important for performance?\n");
    printf("Q4. How does the VFS dispatch a read() call to the correct filesystem?\n");
    printf("Q5. What is the difference between ext4 data=ordered and data=journal modes?\n");
    printf("    Which is default and why?\n");
    printf("Q6. Why can hardlinks NOT cross filesystem boundaries, but symlinks can?\n");
    printf("Q7. How does btrfs achieve crash consistency without a traditional journal\n");
    printf("    (hint: copy-on-write and B-tree transactions)?\n");
    return 0;
}
