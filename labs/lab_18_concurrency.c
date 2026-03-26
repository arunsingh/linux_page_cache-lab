/*
 * lab_18.c
 * Topic: Handling user pointers, concurrency
 * Build: gcc -O0 -Wall -pthread -o lab_18 lab_18_*.c
 *
 * Author: Arun Singh | arunsingh.in@gmail.com
 * OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdatomic.h>
#include <unistd.h>
static void print_section(const char *t){printf("\n========== %s ==========\n",t);}
static volatile long counter_racy=0;
static atomic_long counter_safe=0;
static void *racy_inc(void *arg){
    int n=*(int*)arg;
    for(int i=0;i<n;i++)counter_racy++;
    return NULL;
}
static void *safe_inc(void *arg){
    int n=*(int*)arg;
    for(int i=0;i<n;i++)atomic_fetch_add(&counter_safe,1);
    return NULL;
}
int main(void){
    printf("=== Lab 18: User Pointers, Concurrency ===\n");
    print_section("Phase 1: Data Race Demonstration");
    int N=1000000,T=4;
    pthread_t threads[4];int args[4];
    for(int i=0;i<T;i++){args[i]=N;pthread_create(&threads[i],NULL,racy_inc,&args[i]);}
    for(int i=0;i<T;i++)pthread_join(threads[i],NULL);
    printf("  Expected: %d, Got: %ld (LOST %ld updates!)\n",N*T,counter_racy,(long)N*T-counter_racy);
    print_section("Phase 2: Atomic Fix");
    for(int i=0;i<T;i++){args[i]=N;pthread_create(&threads[i],NULL,safe_inc,&args[i]);}
    for(int i=0;i<T;i++)pthread_join(threads[i],NULL);
    printf("  Expected: %d, Got: %ld (correct with atomics)\n",N*T,atomic_load(&counter_safe));
    print_section("Phase 3: User Pointer Safety in Kernel");
    printf("  When kernel handles syscalls, user pointers must be validated:\n");
    printf("    copy_from_user() / copy_to_user() — checks address range + handles faults\n");
    printf("    access_ok() — verifies pointer is in user address space\n");
    printf("  Without these checks: user could trick kernel into reading/writing kernel memory.\n");
    printf("  This was the basis of many privilege escalation exploits.\n");
    printf("\n========== Quiz ==========\n");
    printf("Q1. Why did the racy counter lose updates? Describe the interleaving.\n");
    printf("Q2. What does atomic_fetch_add compile down to on x86? (hint: LOCK XADD)\n");
    printf("Q3. Why must the kernel validate user pointers before dereferencing them?\n");
    printf("Q4. What is SMAP/SMEP and how do they protect against user pointer attacks?\n");
    return 0;
}
