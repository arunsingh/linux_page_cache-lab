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
    printf("\n========== Quiz ==========\n");
    printf("Q1. What is an inode and what metadata does it store?\n");
    printf("Q2. Why does NVMe often use the 'none' I/O scheduler?\n");
    printf("Q3. What is the difference between f_bfree and f_bavail in statvfs?\n");
    printf("Q4. How does the VFS layer abstract different filesystems?\n");
    return 0;}
