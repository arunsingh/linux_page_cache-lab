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

/* WHY: Thread-Local Storage (TLS) is a per-thread segment (%fs on x86_64) that holds
 *      errno, thread ID, and user-defined __thread variables.  Each thread's %fs base
 *      register is set by the kernel via arch_prctl(ARCH_SET_FS) during clone().
 *      In Linux 6.x this is surfaced as a distinct VMA with [anon] tag in /proc/maps.
 *
 * WHY: cgroups v2 (unified hierarchy, default since kernel 5.x) lets containers
 *      enforce memory.max, memory.swap.max per-group.  The kernel's memory controller
 *      tracks RSS+swap per cgroup; OOM killer is cgroup-scoped in v2.
 *      Docker and Kubernetes both use cgroups v2 for resource accounting.
 */

/* --- Phase 5: Count and classify /proc/self/maps regions --- */

/* OBSERVE: Each distinct pathname (or anonymous) in /proc/self/maps is a VMA.
 *          Counting them reveals the minimum address space "footprint" a thread has. */
static void demo_count_vma_regions(void) {
    print_section("Phase 5: Count Unique Address Space Regions");

    FILE *fp = fopen("/proc/self/maps", "r");
    if (!fp) { perror("fopen /proc/self/maps"); return; }

    int total = 0, anon = 0, file_backed = 0, special = 0;
    int has_vdso = 0, has_vvar = 0, has_heap = 0, has_stack = 0;
    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        total++;
        if (strstr(line, "[vdso]"))       { has_vdso  = 1; special++; }
        else if (strstr(line, "[vvar]"))  { has_vvar  = 1; special++; }
        else if (strstr(line, "[heap]"))  { has_heap  = 1; special++; }
        else if (strstr(line, "[stack]")) { has_stack = 1; special++; }
        else if (strstr(line, "/"))       file_backed++;
        else                              anon++;
    }
    fclose(fp);

    printf("  Total VMAs     : %d\n", total);
    printf("  File-backed    : %d  (executable, shared libs, mmap'd files)\n", file_backed);
    printf("  Anonymous      : %d  (heap, thread stacks, mmap(MAP_ANON))\n", anon);
    printf("  Special kernel : %d  (vdso=%d, vvar=%d, heap=%d, stack=%d)\n",
           special, has_vdso, has_vvar, has_heap, has_stack);

    /* WHY: [vdso] is a kernel-exported shared library mapped into every process.
     *      It implements clock_gettime(), gettimeofday(), and getcpu() without a
     *      full syscall — the kernel updates a shared page the process reads directly.
     *      [vvar] is the read-only data page the vdso code reads (TSC offset, etc.). */
    printf("\n  WHY vdso: kernel maps ~4 KiB of code into every process so that\n");
    printf("            clock_gettime() never crosses the user/kernel boundary.\n");
    printf("            This cuts latency from ~100 ns (syscall) to ~5 ns (vdso).\n");

    /* WHY: cgroups v2 memory.max limits the total memory (anon + file) a cgroup can use.
     *      When a container hits memory.max, the kernel invokes the cgroup-local OOM
     *      killer rather than killing processes system-wide. */
    char cg_mem[128] = "/sys/fs/cgroup/memory.max";
    fp = fopen(cg_mem, "r");
    if (fp) {
        char val[64] = {0};
        if (fgets(val, sizeof(val), fp)) {
            val[strcspn(val, "\n")] = '\0';
            printf("\n  cgroups v2 memory.max for this process: %s\n", val);
            printf("  (\"max\" means unlimited; containers set a numeric byte limit)\n");
        }
        fclose(fp);
    }
}

int main(void) {
    /* EXPECTED OUTPUT (Linux x86_64):
     *   === Lab 01: Threads, Address Spaces, Filesystem Devices ===
     *   PID: 12345
     *   === Phase 1: Threads Share Address Space ===
     *   Main: PID=12345, TID=12345
     *   Main: &shared_global=0x... (same address seen by all 3 threads)
     *   Thread 0/1/2: same &shared_global address, different TIDs
     *   After all threads: global=45, heap_var=130   (42+3 and 100+3*10)
     *   === Phase 2: Processes Have Isolated Address Spaces ===
     *   Child modifies test_var to 999; Parent still sees 500  (COW)
     *   === Phase 5: Count Unique Address Space Regions ===
     *   Total VMAs: 20-40 (more with ASLR + many libs loaded)
     *   File-backed: ~10-25, Anonymous: ~5-10, Special kernel: 4
     */
    printf("=== Lab 01: Threads, Address Spaces, Filesystem Devices ===\n");
    printf("PID: %d\n", getpid());

    demo_threads_shared_space();
    demo_process_isolation();
    demo_address_space_map();
    demo_fs_devices();
    demo_count_vma_regions();

    /* === EXERCISE ===
     * Try these hands-on tasks:
     * 1. Read /proc/self/maps in a loop BEFORE and AFTER calling malloc(1<<20).
     *    Run: grep '\[heap\]' /proc/<PID>/maps both times and note how the heap
     *    end address changes.  This shows sbrk()/brk() in action.
     * 2. Create a new thread and inside the thread function open /proc/self/maps
     *    and grep for lines that do NOT appear in the main thread's maps snapshot.
     *    The new entries are the thread's TLS block and stack VMA.
     *    Measure: does TLS appear as file-backed or anonymous?
     * 3. Modern (cgroups v2 + namespaces): run this binary inside a Docker container
     *    with --memory=64m and check /sys/fs/cgroup/memory.max from inside the
     *    container.  Then malloc() more than 64 MiB and observe the OOM kill message.
     *    Compare: without --memory limit the same allocation succeeds silently.
     *
     * OBSERVE: Thread stacks show as anonymous VMAs with rw-p permissions and no
     *          pathname.  Their size is typically 8 MiB (ulimit -s default) but the
     *          kernel only faults in pages actually touched (guard page at bottom).
     * WHY:     The kernel creates a separate VMA for each thread stack and TLS block
     *          during clone(CLONE_VM|CLONE_THREAD).  This is why the VMA count grows
     *          by ~2-3 for every additional thread you create.
     */
    printf("\n========== Hands-On Exercise ==========\n");
    printf("1. Read /proc/self/maps before and after malloc(1<<20); observe [heap] growth.\n");
    printf("2. In a new thread, open /proc/self/maps and find the thread stack VMA.\n");
    printf("3. Modern: run in Docker --memory=64m, read /sys/fs/cgroup/memory.max,\n");
    printf("   then malloc >64 MiB and observe cgroup-scoped OOM kill.\n");

    printf("\n========== Quiz ==========\n");
    printf("Q1. Do threads in the same process share the same virtual address space? How did you verify?\n");
    printf("Q2. When a child process modifies a variable, does the parent see the change? Why?\n");
    printf("Q3. What is the difference between a character device and a block device?\n");
    printf("Q4. What does the [vdso] mapping represent and why does the kernel provide it?\n");
    printf("Q5. If two threads read the same global variable, do they access the same physical memory? Why?\n");
    printf("Q6. What is Thread-Local Storage (TLS) and how does the kernel set it up on x86_64\n");
    printf("    (hint: %%fs segment register and arch_prctl)?\n");
    printf("Q7. In cgroups v2, how does memory.max differ from the older cgroups v1\n");
    printf("    memory.limit_in_bytes, and why does Kubernetes prefer cgroups v2?\n");
    return 0;
}
