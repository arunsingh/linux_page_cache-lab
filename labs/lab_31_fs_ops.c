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
    printf("\n========== Quiz ==========\n");
    printf("Q1. Why is write() so much faster than fsync()? Where does data go?\n");
    printf("Q2. What data can be lost if the system crashes before fsync()?\n");
    printf("Q3. What is the difference between fsync() and fdatasync()?\n");
    printf("Q4. Why would a database use O_DIRECT?\n");
    return 0;}
