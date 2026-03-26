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
    printf("\n========== Quiz ==========\n");
    printf("Q1. What is the difference between an inode and a dentry?\n");
    printf("Q2. Can two dentries point to the same inode? When does this happen?\n");
    printf("Q3. What does the dentry cache do and why is it important for performance?\n");
    printf("Q4. How does the VFS dispatch a read() call to the correct filesystem?\n");
    return 0;}
