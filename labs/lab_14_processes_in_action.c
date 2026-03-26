/*
 * lab_14.c
 * Topic: Processes in action
 * Build: gcc -O0 -Wall -pthread -o lab_14 lab_14_*.c
 *
 * Author: Arun Singh | arunsingh.in@gmail.com
 * OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
static void print_section(const char *t){printf("\n========== %s ==========\n",t);}
int main(void){
    printf("=== Lab 14: Processes in Action ===\n");
    print_section("Phase 1: Fork, Exec, Wait");
    printf("  Parent PID: %d, PPID: %d\n",getpid(),getppid());
    for(int i=0;i<3;i++){
        pid_t p=fork();
        if(p==0){
            printf("  Child %d: PID=%d PPID=%d\n",i,getpid(),getppid());
            if(i==2){
                printf("  Child 2: exec(ls)...\n");
                execlp("ls","ls","-la","/proc/self",NULL);
                perror("exec");
            }
            _exit(i);
        }
    }
    for(int i=0;i<3;i++){
        int status;pid_t w=wait(&status);
        if(WIFEXITED(status))printf("  Reaped PID %d, exit=%d\n",w,WEXITSTATUS(status));
    }
    print_section("Phase 2: Process Tree");
    printf("  Run: pstree -p %d\n",getpid());
    printf("  Or:  cat /proc/%d/status | grep -E \"Pid|PPid|Threads\"\n",getpid());
    print_section("Phase 3: Zombie and Orphan");
    pid_t z=fork();
    if(z==0)_exit(0);
    printf("  Created child PID %d — now a zombie until we wait().\n",z);
    printf("  Check: ps aux | grep Z | grep %d\n",z);
    usleep(100000);
    wait(NULL);
    printf("  Reaped zombie.\n");
    printf("\n========== Quiz ==========\n");
    printf("Q1. What is a zombie process and how is it created?\n");
    printf("Q2. What happens to orphaned processes?\n");
    printf("Q3. What does exec() replace in the process? What does it preserve?\n");
    printf("Q4. Why is fork()+exec() the standard pattern instead of a single spawn()?\n");
    return 0;
}
