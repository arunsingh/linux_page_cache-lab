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
    printf("\n========== Quiz ==========\n");
    printf("Q1. What is jbd2 and how does it relate to ext4?\n");
    printf("Q2. What is a transaction in the ext4 journal?\n");
    printf("Q3. Why are disk write barriers important for journal integrity?\n");
    printf("Q4. What is FUA and how does NVMe handle it differently from SATA?\n");
    return 0;}
