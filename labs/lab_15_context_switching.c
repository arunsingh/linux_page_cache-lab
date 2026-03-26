/*
 * lab_15.c
 * Topic: Process structure, Context switching
 * Build: gcc -O0 -Wall -pthread -o lab_15 lab_15_*.c
 *
 * Author: Arun Singh | arunsingh.in@gmail.com
 * OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sched.h>
#include <pthread.h>
static void print_section(const char *t){printf("\n========== %s ==========\n",t);}
static double now_ns(void){struct timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);return ts.tv_sec*1e9+ts.tv_nsec;}
static void demo_ctx_switch_cost(void){
    print_section("Phase 1: Context Switch Cost Measurement");
    int p1[2],p2[2];
    pipe(p1);pipe(p2);
    char c=0;int N=100000;
    pid_t pid=fork();
    if(pid==0){
        for(int i=0;i<N;i++){read(p1[0],&c,1);write(p2[1],&c,1);}
        _exit(0);
    }
    double t0=now_ns();
    for(int i=0;i<N;i++){write(p1[1],&c,1);read(p2[0],&c,1);}
    double elapsed=now_ns()-t0;
    wait(NULL);
    printf("  %d round-trips via pipe: %.0f ns total\n",N,elapsed);
    printf("  Per context switch: ~%.0f ns (%.1f us)\n",elapsed/(2.0*N),elapsed/(2000.0*N));
    printf("  Modern server: typically 1-5 us per context switch.\n");
}
static void *thread_ping(void *arg){
    int *fds=(int*)arg;char c=0;
    for(int i=0;i<100000;i++){read(fds[0],&c,1);write(fds[1],&c,1);}
    return NULL;
}
static void demo_thread_switch(void){
    print_section("Phase 2: Thread vs Process Context Switch");
    int p1[2],p2[2];pipe(p1);pipe(p2);
    int fds[2]={p1[0],p2[1]};
    pthread_t t;char c=0;int N=100000;
    pthread_create(&t,NULL,thread_ping,fds);
    double t0=now_ns();
    for(int i=0;i<N;i++){write(p1[1],&c,1);read(p2[0],&c,1);}
    double elapsed=now_ns()-t0;
    pthread_join(t,NULL);
    printf("  Thread switch: ~%.0f ns per switch\n",elapsed/(2.0*N));
    printf("  OBSERVE: Thread switches are often faster — no TLB flush, shared mm.\n");
}
static void demo_voluntary_stats(void){
    print_section("Phase 3: Context Switch Stats");
    char path[128];
    snprintf(path,sizeof(path),"/proc/%d/status",getpid());
    FILE *fp=fopen(path,"r");
    if(!fp)return;
    char line[256];
    while(fgets(line,sizeof(line),fp)){
        if(strstr(line,"ctxt_switches")||strstr(line,"voluntary"))printf("  %s",line);
    }
    fclose(fp);
    printf("  voluntary = process yielded CPU (I/O, sleep, mutex)\n");
    printf("  nonvoluntary = preempted by scheduler (time slice expired)\n");
}
int main(void){
    printf("=== Lab 15: Process Structure, Context Switching ===\n");
    demo_ctx_switch_cost();
    demo_thread_switch();
    demo_voluntary_stats();
    printf("\n========== Quiz ==========\n");
    printf("Q1. What state must the kernel save/restore during a context switch?\n");
    printf("Q2. Why are thread context switches cheaper than process switches?\n");
    printf("Q3. What is a voluntary vs nonvoluntary context switch?\n");
    printf("Q4. How does context switch cost affect GPU DC performance (hint: NCCL, latency)?\n");
    return 0;
}
