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
    printf("\n========== Quiz ==========\n");
    printf("Q1. What is write-ahead logging and why is it needed?\n");
    printf("Q2. What can go wrong with rename() without fsync on the directory?\n");
    printf("Q3. What is the difference between ext4 ordered and writeback modes?\n");
    printf("Q4. How does journaling relate to database WAL (e.g., PostgreSQL)?\n");
    return 0;}
