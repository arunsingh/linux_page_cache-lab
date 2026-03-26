/*
 * lab_33_ext4_journal.c
 * Topic: ext4 Journal
 * Build: gcc -O0 -Wall -pthread -o lab_33 lab_33_ext4_journal.c
 *
 * Author: Arun Singh | arunsingh.in@gmail.com
 * OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
*/
#include <stdio.h>
#include <string.h>
#include <unistd.h>
static void ps(const char *t){printf("\n========== %s ==========\n",t);}
int main(void){
    printf("=== Lab 33: Logging in ext4 Filesystem ===\n");
    ps("Phase 1: ext4 Features");
    FILE *fp=popen("mount | grep ext4 | head -3","r");
    if(fp){char l[256];while(fgets(l,sizeof(l),fp))printf("  %s",l);pclose(fp);}
    else printf("  (no ext4 mounts found — check with: mount | grep ext)\n");
    ps("Phase 2: Journal Device");
    printf("  ext4 journal (jbd2) creates a circular log on disk.\n");
    printf("  Transactions: group related changes, commit atomically.\n");
    printf("  Checkpointing: flush committed transactions to their final locations.\n\n");
    fp=popen("cat /proc/fs/jbd2/*/info 2>/dev/null | head -20","r");
    if(fp){char l[256];int n=0;while(fgets(l,sizeof(l),fp)&&n++<15)printf("  %s",l);pclose(fp);}
    ps("Phase 3: Barriers");
    printf("  Disk write barriers ensure journal commit hits disk before data.\n");
    printf("  Without barriers: disk reordering can corrupt the journal.\n");
    printf("  NVMe: supports FUA (Force Unit Access) for barrier-like semantics.\n");
    /* WHY: ext4 "fast commit" (Linux 5.10+) is a lightweight journal optimization.
     *      Traditional jbd2 commits write a full "descriptor block" listing all
     *      changed blocks, then all changed data.  Fast commit writes only the delta
     *      (which files changed and how) using a compact format.  This reduces
     *      journal commit size by 80-90% for common operations (append, rename, link).
     *      Enabled with: tune2fs -O fast_commit /dev/sdX
     *      Benchmark: fast commit improves fsync() throughput for small random writes.
     *
     * WHY: ext4 inline data: for files < 60 bytes, the file content is stored directly
     *      in the inode itself (in the i_block[] field which normally holds block pointers).
     *      This eliminates the need for a separate data block, saving one disk I/O.
     *      Enabled with: tune2fs -O inline_data /dev/sdX
     *      Common for: /proc files, config snippets, symlink targets (< 60 chars).
     */
    ps("Phase 4: Modern ext4 Features (fast_commit, inline_data)");
    printf("  ext4 fast_commit (Linux 5.10+):\n");
    printf("    Traditional: full descriptor block per commit (all modified blocks listed)\n");
    printf("    fast_commit: write only the 'delta' of what changed (smaller, faster)\n");
    printf("    Benefit: 80-90%% smaller journal writes for append/rename/link operations\n");
    printf("    Enable: tune2fs -O fast_commit /dev/sdX\n\n");
    printf("  ext4 inline_data:\n");
    printf("    Files < 60 bytes stored in the inode's i_block[] area directly\n");
    printf("    No separate data block needed; saves one disk I/O per small file\n");
    printf("    Enable: tune2fs -O inline_data /dev/sdX\n\n");

    /* Check /proc/fs/ext4/ for live journal stats */
    ps("Phase 5: Live ext4 Journal Stats (/proc/fs/ext4/)");
    {
        FILE *fp2;
        /* Try to list ext4 filesystem entries */
        fp2 = popen("ls /proc/fs/ext4/ 2>/dev/null", "r");
        if (fp2) {
            char l[256]; int found = 0;
            printf("  ext4 filesystems with stats in /proc/fs/ext4/:\n");
            while (fgets(l, sizeof(l), fp2)) {
                l[strcspn(l, "\n")] = '\0';
                printf("    /proc/fs/ext4/%s/\n", l);
                found = 1;
            }
            pclose(fp2);
            if (!found) printf("    (none -- no ext4 mounted, or /proc/fs/ext4 unavailable)\n");
        }
        /* Try to read journal stats for first ext4 filesystem */
        fp2 = popen("cat /proc/fs/ext4/*/mb_groups 2>/dev/null | head -5", "r");
        if (fp2) {
            char l[256];
            printf("  ext4 block group info (first 5 lines of mb_groups):\n");
            while (fgets(l, sizeof(l), fp2)) printf("    %s", l);
            pclose(fp2);
        }
    }

    /* === EXERCISE ===
     * Try these hands-on tasks:
     * 1. Read /proc/fs/jbd2/*/info for a live ext4 filesystem.  Identify:
     *    - Journal size (j_maxlen blocks)
     *    - Current transaction handle count (j_running_transaction)
     *    - Checkpointing progress
     *    Then run a workload (create 1000 files) and re-read to see changes.
     * 2. Compare fsync() throughput with and without fast_commit:
     *    Create a test filesystem on a loop device:
     *      dd if=/dev/zero of=/tmp/testfs.img bs=1M count=256
     *      mkfs.ext4 /tmp/testfs.img
     *    Mount, benchmark 10K small file creates + fsyncs.
     *    Then: tune2fs -O fast_commit /tmp/testfs.img, remount, benchmark again.
     * 3. Modern (inline data + ordered journaling): create 10K small files (<60B) on
     *    an ext4 filesystem with inline_data enabled.  Compare disk space used vs
     *    without inline_data (stat each file to see blocks=0 for inlined files).
     *    Explain: inline_data eliminates the data block for tiny files, saving ~4KB per file.
     *
     * OBSERVE: jbd2 groups multiple fsync requests into one transaction if they
     *          arrive within the commit interval (default 5 seconds).  This is "group commit":
     *          10 threads calling fsync simultaneously may share one journal commit.
     *          PostgreSQL uses this to improve throughput under concurrent write load.
     * WHY:     The journal's commit record (with a 32-bit checksum in ext4 crc32c mode)
     *          is the ONLY record that makes a transaction durable.  If the commit record
     *          is not on disk, the transaction is replayed-and-discarded on recovery.
     */
    printf("\n========== Hands-On Exercise ==========\n");
    printf("1. Read /proc/fs/jbd2/*/info; run 1000-file workload; observe counter changes.\n");
    printf("2. Create loop-device ext4; benchmark fsync throughput without/with fast_commit.\n");
    printf("3. Modern (inline_data): create files < 60B on inline_data ext4;\n");
    printf("   verify blocks=0 in stat output (content stored in inode).\n");

    printf("\n========== Quiz ==========\n");
    printf("Q1. What is jbd2 and how does it relate to ext4?\n");
    printf("Q2. What is a transaction in the ext4 journal?\n");
    printf("Q3. Why are disk write barriers important for journal integrity?\n");
    printf("Q4. What is FUA and how does NVMe handle it differently from SATA?\n");
    printf("Q5. What is ext4 fast_commit and how does it differ from traditional jbd2 commits?\n");
    printf("Q6. What is ext4 inline_data and for what file size range does it apply?\n");
    printf("Q7. How does jbd2 group commit improve fsync() throughput under concurrent\n");
    printf("    write load (hint: multiple fsync callers share one journal transaction)?\n");
    return 0;
}
