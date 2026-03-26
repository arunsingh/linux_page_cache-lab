/*
 * lab_01_threads_addr_fs.c
 *
 * Topic: Threads, Address Spaces, Filesystem Devices
 *
 * Demonstrates:
 *   1. Thread creation and shared address space verification
 *   2. Reading /proc/self/maps to see address space layout
 *   3. Listing /dev entries to understand device files
 *   4. Thread vs process address space isolation
 *
 * Build: gcc -O0 -Wall -pthread -o lab_01 lab_01_threads_addr_fs.c
 * Run:   ./lab_01
 *
 * Author: Arun Singh | arunsingh.in@gmail.com
 * OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include <sys/stat.h>

static int shared_global = 42;

static void print_section(const char *title) {
    printf("\n========== %s ==========\n", title);
}

/* --- Phase 1: Thread shared address space --- */

typedef struct {
    int id;
    int *shared_ptr;
} thread_arg_t;

static void *thread_func(void *arg) {
    thread_arg_t *ta = (thread_arg_t *)arg;
    printf("  Thread %d: PID=%d, TID=%ld\n", ta->id, getpid(), (long)gettid());
    printf("  Thread %d: &shared_global=%p, value=%d\n", ta->id, (void *)&shared_global, shared_global);
    printf("  Thread %d: shared_ptr points to %p, value=%d\n", ta->id, (void *)ta->shared_ptr, *ta->shared_ptr);

    /* Modify shared state */
    __sync_fetch_and_add(&shared_global, 1);
    __sync_fetch_and_add(ta->shared_ptr, 10);

    printf("  Thread %d: After modification: global=%d, *shared_ptr=%d\n",
           ta->id, shared_global, *ta->shared_ptr);
    return NULL;
}

static void demo_threads_shared_space(void) {
    print_section("Phase 1: Threads Share Address Space");
    printf("Main: PID=%d, TID=%ld\n", getpid(), (long)gettid());
    printf("Main: &shared_global=%p, value=%d\n", (void *)&shared_global, shared_global);

    int heap_var = 100;
    printf("Main: &heap_var=%p (stack), value=%d\n\n", (void *)&heap_var, heap_var);

    pthread_t threads[3];
    thread_arg_t args[3];

    for (int i = 0; i < 3; i++) {
        args[i].id = i;
        args[i].shared_ptr = &heap_var;
        pthread_create(&threads[i], NULL, thread_func, &args[i]);
    }
    for (int i = 0; i < 3; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("\nMain after all threads: global=%d, heap_var=%d\n", shared_global, heap_var);
    printf("\nOBSERVE: All threads see the SAME addresses for global and stack variables.\n");
    printf("         All threads share PID but have different TIDs.\n");
    printf("         Modifications by one thread are visible to all others.\n");
}

/* --- Phase 2: Process isolation contrast --- */

static void demo_process_isolation(void) {
    print_section("Phase 2: Processes Have Isolated Address Spaces");

    int test_var = 500;
    printf("Parent: PID=%d, &test_var=%p, value=%d\n", getpid(), (void *)&test_var, test_var);

    pid_t pid = fork();
    if (pid == 0) {
        /* Child */
        test_var = 999;
        printf("Child:  PID=%d, &test_var=%p, value=%d (modified)\n", getpid(), (void *)&test_var, test_var);
        printf("Child:  shared_global=%d (copied, not shared)\n", shared_global);
        _exit(0);
    }
    wait(NULL);
    printf("Parent: test_var=%d (unchanged despite child modification)\n", test_var);
    printf("\nOBSERVE: Child gets a COPY of the address space (COW).\n");
    printf("         The virtual addresses may look the same but map to different physical pages.\n");
}

/* --- Phase 3: Address space map --- */

static void demo_address_space_map(void) {
    print_section("Phase 3: Process Address Space Layout (/proc/self/maps)");

    printf("Key regions in a process address space:\n\n");
    FILE *fp = fopen("/proc/self/maps", "r");
    if (!fp) { perror("fopen /proc/self/maps"); return; }

    char line[512];
    int count = 0;
    while (fgets(line, sizeof(line), fp) && count < 20) {
        /* Show first 20 mappings */
        printf("  %s", line);
        count++;
    }
    if (count >= 20) printf("  ... (truncated, see /proc/%d/maps for full listing)\n", getpid());
    fclose(fp);

    printf("\nOBSERVE: Each line shows: address_range permissions offset device inode pathname\n");
    printf("         [heap]  = dynamically allocated memory (malloc, etc.)\n");
    printf("         [stack] = thread stack\n");
    printf("         [vdso]  = virtual dynamic shared object (kernel-provided fast syscalls)\n");
    printf("         r-xp    = read+execute (code), rw-p = read+write (data)\n");
}

/* --- Phase 4: Filesystem devices --- */

static void demo_fs_devices(void) {
    print_section("Phase 4: Filesystem Device Files (/dev)");

    printf("Device files provide userspace interface to kernel drivers.\n");
    printf("Two types: character devices (c) and block devices (b).\n\n");

    const char *interesting[] = {
        "/dev/null", "/dev/zero", "/dev/urandom", "/dev/tty",
        "/dev/sda", "/dev/loop0", "/dev/pts/0", NULL
    };

    struct stat st;
    for (int i = 0; interesting[i]; i++) {
        if (stat(interesting[i], &st) == 0) {
            char type = S_ISCHR(st.st_mode) ? 'c' : S_ISBLK(st.st_mode) ? 'b' : '?';
            printf("  %s  type=%c  major=%d  minor=%d\n",
                   interesting[i], type,
                   (int)(st.st_rdev >> 8) & 0xff,
                   (int)(st.st_rdev & 0xff));
        }
    }

    printf("\nOBSERVE: /dev/null discards writes, /dev/zero provides infinite zeros,\n");
    printf("         /dev/urandom provides random bytes.\n");
    printf("         Major number identifies the driver, minor identifies the instance.\n");
}

int main(void) {
    printf("=== Lab 01: Threads, Address Spaces, Filesystem Devices ===\n");
    printf("PID: %d\n", getpid());

    demo_threads_shared_space();
    demo_process_isolation();
    demo_address_space_map();
    demo_fs_devices();

    printf("\n========== Quiz ==========\n");
    printf("Q1. Do threads in the same process share the same virtual address space? How did you verify?\n");
    printf("Q2. When a child process modifies a variable, does the parent see the change? Why?\n");
    printf("Q3. What is the difference between a character device and a block device?\n");
    printf("Q4. What does the [vdso] mapping represent and why does the kernel provide it?\n");
    printf("Q5. If two threads read the same global variable, do they access the same physical memory? Why?\n");
    return 0;
}
