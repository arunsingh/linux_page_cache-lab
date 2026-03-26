/*
 * process_memory_exercises.c
 *
 * Topic: Process & Memory Subsystems Deep Dive
 *
 * A consolidated exercise covering the kernel subsystems that underpin all 40 labs:
 *   1.  task_struct fields visible via /proc/<pid>/status
 *   2.  mm_struct: VMA list, RSS, PSS, page table cost
 *   3.  Page fault lifecycle: mmap -> first-touch -> warm rescan
 *   4.  mmap type comparison: anonymous, file-backed, shared, private
 *   5.  OOM killer scoring (/proc/<pid>/oom_score)
 *   6.  Huge page allocation and TLB impact
 *   7.  /proc/meminfo full tour
 *   8.  Kernel thread observation via /proc
 *   9.  Process lineage: fork -> exec -> wait chain
 *   10. cgroup memory limit detection
 *
 * Build:  gcc -O0 -Wall -pthread -o bin/pm_exercises labs/process_memory_exercises.c
 * Run:    ./bin/pm_exercises
 *
 * Inspired by: https://iitd-os.github.io/os-nptel/
 *              https://www.cse.iitd.ac.in/~sbansal/os/
 *
 * Author: Arun Singh | arunsingh.in@gmail.com
 * OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <time.h>

/* ── helpers ─────────────────────────────────────────────────────────────── */

static void section(const char *title) {
    printf("\n╔══════════════════════════════════════════════════════════════╗\n");
    printf("║  %-60s║\n", title);
    printf("╚══════════════════════════════════════════════════════════════╝\n");
}

static void subsection(const char *s) {
    printf("\n  ── %s ──\n", s);
}

static void print_proc_file(const char *path, int max_lines) {
    FILE *f = fopen(path, "r");
    if (!f) { printf("  [cannot open %s: %s]\n", path, strerror(errno)); return; }
    char line[512];
    int n = 0;
    while (fgets(line, sizeof line, f) && n < max_lines) {
        printf("  %s", line);
        n++;
    }
    if (!feof(f)) printf("  ... (truncated at %d lines)\n", max_lines);
    fclose(f);
}

static void print_status_field(const char *field) {
    char path[64];
    snprintf(path, sizeof path, "/proc/%d/status", getpid());
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof line, f)) {
        if (strncmp(line, field, strlen(field)) == 0) {
            printf("  %s", line);
            break;
        }
    }
    fclose(f);
}

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

static void get_faults(long *mn, long *mj) {
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);
    *mn = ru.ru_minflt;
    *mj = ru.ru_majflt;
}

/* ── Exercise 1: task_struct fields ─────────────────────────────────────── */

static void ex_task_struct(void) {
    section("Exercise 1: task_struct Fields via /proc/<pid>/status");

    printf("  WHAT: /proc/<pid>/status exposes key task_struct fields in human-readable form.\n");
    printf("  WHY:  task_struct (~10 KB in Linux 6.x) is the kernel's process descriptor.\n");
    printf("        Every field here maps directly to a struct member in include/linux/sched.h\n\n");

    char path[64];
    snprintf(path, sizeof path, "/proc/%d/status", getpid());

    const char *fields[] = {
        "Name", "Pid", "PPid", "Threads", "VmPeak", "VmSize",
        "VmRSS", "VmData", "VmStk", "VmExe", "VmPTE",
        "voluntary_ctxt_switches", "nonvoluntary_ctxt_switches", NULL
    };

    FILE *f = fopen(path, "r");
    if (!f) { perror("fopen status"); return; }

    char line[256];
    while (fgets(line, sizeof line, f)) {
        for (int i = 0; fields[i]; i++) {
            if (strncmp(line, fields[i], strlen(fields[i])) == 0) {
                printf("  %s", line);
                break;
            }
        }
    }
    fclose(f);

    printf("\n  OBSERVE:\n");
    printf("    VmSize  = reserved virtual address space (vm_area_struct list total)\n");
    printf("    VmRSS   = resident pages actually in RAM (physical frames mapped)\n");
    printf("    VmPTE   = memory used by page table entries themselves\n");
    printf("    VmStk   = size of main thread stack VMA\n");
    printf("    voluntary_ctxt_switches   = syscall/sleep caused rescheduling\n");
    printf("    nonvoluntary_ctxt_switches = scheduler preempted this task\n");

    printf("\n  QUIZ:\n");
    printf("    Q1. What is the difference between VmSize and VmRSS?\n");
    printf("    Q2. Why does VmPTE grow as you mmap() more regions?\n");
    printf("    Q3. What kernel struct does each /proc/<pid>/status line map to?\n");
}

/* ── Exercise 2: mm_struct — VMA list ───────────────────────────────────── */

static void ex_mm_struct(void) {
    section("Exercise 2: mm_struct and the VMA List (/proc/self/maps + smaps)");

    printf("  WHAT: mm_struct is the per-process virtual memory descriptor.\n");
    printf("  WHY:  It holds the VMA red-black tree, page table root (pgd),\n");
    printf("        mm_count/mm_users, and memory statistics.\n\n");

    /* allocate a fresh anonymous region so it appears in maps */
    size_t sz = 2 * 1024 * 1024; /* 2 MiB */
    void *anon = mmap(NULL, sz, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (anon == MAP_FAILED) { perror("mmap"); return; }
    memset(anon, 0xAB, sz);   /* touch all pages -> RSS = 2 MiB */

    subsection("/proc/self/maps  (first 25 lines)");
    print_proc_file("/proc/self/maps", 25);

    /* find our fresh mapping in smaps */
    subsection("smaps entry for our 2 MiB anonymous region");
    FILE *sf = fopen("/proc/self/smaps", "r");
    if (sf) {
        char line[256], target[32];
        snprintf(target, sizeof target, "%lx", (unsigned long)anon);
        int found = 0, printed = 0;
        while (fgets(line, sizeof line, sf) && printed < 18) {
            if (!found && strstr(line, target)) found = 1;
            if (found) { printf("  %s", line); printed++; }
            if (found && strncmp(line, "VmFlags", 7) == 0) break;
        }
        fclose(sf);
    }

    printf("\n  OBSERVE:\n");
    printf("    Size:       = VMA size (reserved virtual space)\n");
    printf("    Rss:        = physical pages currently mapped\n");
    printf("    Pss:        = proportional share (RSS / sharing_count)\n");
    printf("    Private_Dirty: pages written and not shared (COW happened)\n");
    printf("    Shared_Clean:  pages shared read-only (e.g. libc text)\n");
    printf("    Anonymous:  = not backed by a file\n");
    printf("    VmFlags: rd wr mr mw me ac  (read/write/mayread/maywrite/mayexec/accounting)\n");

    munmap(anon, sz);

    printf("\n  QUIZ:\n");
    printf("    Q1. What is PSS and why is it more accurate than RSS for measuring\n");
    printf("        memory usage of a process that uses shared libraries?\n");
    printf("    Q2. After munmap(), does the VMA appear in /proc/self/maps? Why?\n");
    printf("    Q3. What fields in mm_struct track the number of VMAs?\n");
}

/* ── Exercise 3: Page fault lifecycle ───────────────────────────────────── */

static void ex_page_fault_lifecycle(void) {
    section("Exercise 3: Page Fault Lifecycle — mmap -> first-touch -> warm rescan");

    printf("  WHAT: Observe exactly when physical pages are allocated.\n");
    printf("  WHY:  Demand paging means pages are allocated lazily on first access,\n");
    printf("        not at mmap() time. This is fundamental to how fork(), exec(),\n");
    printf("        and every malloc() implementation works.\n\n");

    size_t sz = 32 * 1024 * 1024; /* 32 MiB */
    long mn0, mj0, mn1, mj1;

    /* Step 1: mmap — no pages allocated */
    get_faults(&mn0, &mj0);
    void *mem = mmap(NULL, sz, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) { perror("mmap"); return; }
    get_faults(&mn1, &mj1);

    printf("  [Step 1] mmap(32 MiB, PROT_READ|PROT_WRITE, MAP_ANONYMOUS)\n");
    printf("    minor faults: %ld  major faults: %ld\n", mn1-mn0, mj1-mj0);
    printf("    WHY: mmap() only creates a vm_area_struct. No PTEs, no physical pages.\n");
    print_status_field("VmRSS");

    /* Step 2: read scan — zero page shared */
    get_faults(&mn0, &mj0);
    volatile long sum = 0;
    for (size_t i = 0; i < sz; i += 4096) sum += ((char*)mem)[i];
    get_faults(&mn1, &mj1);

    printf("\n  [Step 2] Read every page (stride 4096)\n");
    printf("    minor faults: %ld  (checksum=%ld)\n", mn1-mn0, sum);
    printf("    WHY: First read maps to the shared zero page — no physical alloc.\n");
    printf("         The zero page is a single global read-only page mapped to all\n");
    printf("         unwritten anonymous pages (saves huge amounts of RAM).\n");
    print_status_field("VmRSS");

    /* Step 3: write — one page per fault */
    get_faults(&mn0, &mj0);
    memset(mem, 0xBB, sz);
    get_faults(&mn1, &mj1);
    long expected = (long)(sz / 4096);

    printf("\n  [Step 3] memset(32 MiB) — first write to each page\n");
    printf("    minor faults: %ld  (expected ~%ld)\n", mn1-mn0, expected);
    printf("    WHY: Each first write triggers COW on the zero page:\n");
    printf("         kernel allocates a real frame, copies zeros, maps PTE R/W.\n");
    print_status_field("VmRSS");

    /* Step 4: warm rescan */
    get_faults(&mn0, &mj0);
    memset(mem, 0xCC, sz);
    get_faults(&mn1, &mj1);

    printf("\n  [Step 4] memset again — warm rescan\n");
    printf("    minor faults: %ld\n", mn1-mn0);
    printf("    WHY: All PTEs are now present. CPU walks page table, no fault.\n");
    print_status_field("VmRSS");

    /* Step 5: MADV_DONTNEED — return pages without munmap */
    madvise(mem, sz, MADV_DONTNEED);
    printf("\n  [Step 5] madvise(MADV_DONTNEED) — release pages without unmapping\n");
    print_status_field("VmRSS");
    printf("    WHY: Kernel marks PTEs not-present and frees frames. VMA survives.\n");
    printf("         This is how jemalloc and tcmalloc return memory to the OS.\n");

    munmap(mem, sz);

    printf("\n  QUIZ:\n");
    printf("    Q1. What is the 'zero page' and why does the kernel maintain it?\n");
    printf("    Q2. Why is MADV_DONTNEED preferred over munmap()+mmap() by allocators?\n");
    printf("    Q3. What does MAP_POPULATE do and when would you use it?\n");
    printf("    Q4. In the write phase, why are all faults 'minor' and not 'major'?\n");
}

/* ── Exercise 4: mmap type comparison ───────────────────────────────────── */

static void ex_mmap_types(void) {
    section("Exercise 4: mmap Type Comparison — Anonymous, File-backed, Shared");

    printf("  WHAT: Different mmap flags produce different VMA types with different\n");
    printf("        kernel behaviours for faults, writeback, and COW.\n\n");

    /* Anonymous private */
    void *anon_priv = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    printf("  MAP_PRIVATE | MAP_ANONYMOUS\n");
    printf("    Use: malloc, stack, BSS\n");
    printf("    Fault: allocates a zero page copy (COW from zero page)\n");
    printf("    Write: stays private — never written back to disk\n\n");

    /* File-backed private (mmap a real file) */
    int fd = open("/proc/self/exe", O_RDONLY);
    void *file_priv = NULL;
    if (fd >= 0) {
        file_priv = mmap(NULL, 4096, PROT_READ, MAP_PRIVATE, fd, 0);
        close(fd);
        printf("  MAP_PRIVATE (file-backed — e.g. text segment from ELF)\n");
        printf("    Use: text/rodata segments of executables and .so files\n");
        printf("    Fault: page read from file on first access\n");
        printf("    Write: COW — modified page becomes private anonymous\n\n");
    }

    /* Shared anonymous — parent/child IPC */
    void *anon_shared = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                             MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    *(volatile int *)anon_shared = 42;
    printf("  MAP_SHARED | MAP_ANONYMOUS\n");
    printf("    Use: shared memory between parent and child after fork()\n");
    printf("    Fault: single physical page shared; writes visible to all mappers\n");
    printf("    No COW: writes go directly to the shared physical page\n\n");

    printf("  OBSERVE in /proc/self/maps:\n");
    FILE *maps = fopen("/proc/self/maps", "r");
    if (maps) {
        char line[256];
        char ap[32], fp[32], sp[32];
        snprintf(ap, sizeof ap, "%lx", (unsigned long)anon_priv);
        if (file_priv && file_priv != MAP_FAILED)
            snprintf(fp, sizeof fp, "%lx", (unsigned long)file_priv);
        else fp[0] = '\0';
        snprintf(sp, sizeof sp, "%lx", (unsigned long)anon_shared);
        while (fgets(line, sizeof line, maps)) {
            if (strstr(line, ap) || (fp[0] && strstr(line, fp)) || strstr(line, sp))
                printf("    %s", line);
        }
        fclose(maps);
    }

    if (anon_priv  != MAP_FAILED) munmap(anon_priv,  4096);
    if (file_priv  && file_priv != MAP_FAILED) munmap(file_priv, 4096);
    if (anon_shared != MAP_FAILED) munmap(anon_shared, 4096);

    printf("\n  QUIZ:\n");
    printf("    Q1. Can MAP_SHARED anonymous memory be used for IPC between unrelated\n");
    printf("        processes (without a file descriptor)? Why or why not?\n");
    printf("    Q2. What happens to dirty pages in a MAP_PRIVATE file-backed mapping\n");
    printf("        when the process exits? What about MAP_SHARED?\n");
    printf("    Q3. Why does 'ps aux' show a much larger VSZ than RSS for most processes?\n");
}

/* ── Exercise 5: OOM killer ─────────────────────────────────────────────── */

static void ex_oom_killer(void) {
    section("Exercise 5: OOM Killer — Scoring and Adjustment");

    printf("  WHAT: When the system runs out of memory, the OOM killer selects\n");
    printf("        and terminates the process with the highest oom_score.\n");
    printf("  WHY:  The score is based on RSS, page table size, and oom_score_adj.\n\n");

    char path[64];

    snprintf(path, sizeof path, "/proc/%d/oom_score", getpid());
    printf("  /proc/self/oom_score: ");
    print_proc_file(path, 1);

    snprintf(path, sizeof path, "/proc/%d/oom_score_adj", getpid());
    printf("  /proc/self/oom_score_adj: ");
    print_proc_file(path, 1);

    printf("\n  oom_score_adj range: -1000 (never kill) to +1000 (kill first)\n");
    printf("  Kubernetes sets oom_score_adj on containers:\n");
    printf("    BestEffort pods:   +1000  (killed first under pressure)\n");
    printf("    Burstable pods:      2-999 (proportional to over-commit)\n");
    printf("    Guaranteed pods:     -998  (protected, killed last)\n\n");

    printf("  Key processes on a Linux server (check with: ");
    printf("cat /proc/<pid>/oom_score_adj):\n");

    const char *check_procs[] = { "1", "2", NULL };
    for (int i = 0; check_procs[i]; i++) {
        char sp[64], sa[64];
        snprintf(sp, sizeof sp, "/proc/%s/oom_score", check_procs[i]);
        snprintf(sa, sizeof sa, "/proc/%s/oom_score_adj", check_procs[i]);
        char cmdline[128] = "(unknown)";
        char cp[64];
        snprintf(cp, sizeof cp, "/proc/%s/comm", check_procs[i]);
        FILE *cf = fopen(cp, "r");
        if (cf) { fgets(cmdline, sizeof cmdline, cf); fclose(cf); cmdline[strcspn(cmdline, "\n")] = 0; }
        printf("  PID %-4s (%s)  oom_score=", check_procs[i], cmdline);
        FILE *f = fopen(sp, "r"); if (f) { char b[16]; fgets(b, sizeof b, f); printf("%s", b); fclose(f); }
        printf("  oom_score_adj=");
        f = fopen(sa, "r"); if (f) { char b[16]; fgets(b, sizeof b, f); printf("%s", b); fclose(f); }
    }

    printf("\n  OBSERVE: PID 1 (init/systemd) has oom_score_adj = -1000 by default.\n");
    printf("           It will never be OOM-killed — doing so would crash the system.\n");

    printf("\n  QUIZ:\n");
    printf("    Q1. How does the kernel calculate oom_score from RSS and page table size?\n");
    printf("    Q2. How would you protect a critical daemon from the OOM killer?\n");
    printf("    Q3. What is the difference between oom_score and oom_score_adj?\n");
    printf("    Q4. How does Kubernetes map QoS classes to oom_score_adj values?\n");
}

/* ── Exercise 6: Huge pages ─────────────────────────────────────────────── */

static void ex_huge_pages(void) {
    section("Exercise 6: Huge Pages — TLB Coverage and Allocation");

    printf("  WHAT: 2 MiB huge pages reduce TLB pressure 512x vs 4 KiB pages.\n");
    printf("  WHY:  A modern Intel dTLB holds ~64 4K-page entries or ~32 2M-page entries.\n");
    printf("        For 256 MiB of random access: 65536 TLB entries needed (4K) vs 128 (2M).\n\n");

    subsection("/proc/meminfo huge page fields");
    FILE *mi = fopen("/proc/meminfo", "r");
    if (mi) {
        char line[256];
        while (fgets(line, sizeof line, mi)) {
            if (strstr(line, "Huge") || strstr(line, "huge") || strstr(line, "AnonHuge"))
                printf("  %s", line);
        }
        fclose(mi);
    }

    subsection("Transparent Huge Pages (THP) setting");
    print_proc_file("/sys/kernel/mm/transparent_hugepage/enabled", 1);

    printf("\n  OBSERVE:\n");
    printf("    HugePages_Total:  pre-allocated huge pages (via vm.nr_hugepages)\n");
    printf("    HugePages_Free:   unused pre-allocated huge pages\n");
    printf("    AnonHugePages:    THP — anonymous huge pages allocated on demand\n");
    printf("    THP enabled=always: kernel promotes 4K pages to 2M automatically\n");
    printf("    THP enabled=madvise: only for regions tagged with MADV_HUGEPAGE\n\n");

    /* Benchmark 4K vs huge-page access (using madvise on Linux) */
    size_t sz = 256 * 1024 * 1024UL;
    void *mem = mmap(NULL, sz, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) { printf("  [mmap failed, skipping benchmark]\n"); return; }

    /* warm up without hugepages */
    memset(mem, 1, sz);
    double t0 = now_sec();
    volatile long s = 0;
    for (size_t i = 0; i < sz; i += 4096) s += ((char*)mem)[i];
    double t4k = now_sec() - t0;
    printf("  Random-stride scan 256 MiB (4K pages): %.3f s  (s=%ld)\n", t4k, s);

#ifdef MADV_HUGEPAGE
    madvise(mem, sz, MADV_HUGEPAGE);
    memset(mem, 2, sz); /* re-touch to trigger THP promotion */
    t0 = now_sec();
    s = 0;
    for (size_t i = 0; i < sz; i += 4096) s += ((char*)mem)[i];
    double tthp = now_sec() - t0;
    printf("  Random-stride scan 256 MiB (THP/2M):   %.3f s  (s=%ld)\n", tthp, s);
    if (tthp > 0 && t4k > 0)
        printf("  Speedup from THP: %.1fx\n", t4k / tthp);
#else
    printf("  [MADV_HUGEPAGE not available on this platform]\n");
#endif

    munmap(mem, sz);

    printf("\n  QUIZ:\n");
    printf("    Q1. Why does THP improve performance for sequential access less than\n");
    printf("        for random access?\n");
    printf("    Q2. What is a TLB shootdown and why is it expensive on 256-core servers?\n");
    printf("    Q3. Why do GPU ML frameworks set THP to 'madvise' rather than 'always'?\n");
    printf("        Hint: khugepaged compaction stalls.\n");
}

/* ── Exercise 7: /proc/meminfo tour ─────────────────────────────────────── */

static void ex_meminfo_tour(void) {
    section("Exercise 7: /proc/meminfo Full Tour");

    printf("  WHAT: A complete view of the system's memory state.\n");
    printf("  WHY:  Every production monitoring tool (Prometheus node_exporter,\n");
    printf("        Datadog, CloudWatch) reads /proc/meminfo. Knowing each field\n");
    printf("        is essential for capacity planning and OOM root-cause analysis.\n\n");

    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) { perror("fopen /proc/meminfo"); return; }

    const struct { const char *key; const char *explanation; } annotations[] = {
        {"MemTotal",      "Total usable RAM (kernel reserved memory excluded)"},
        {"MemFree",       "Completely unused RAM (not cache, not buffers)"},
        {"MemAvailable",  "Estimate of RAM available without swapping (use this, not MemFree)"},
        {"Buffers",       "Kernel block-layer I/O buffers (small, mostly metadata)"},
        {"Cached",        "Page cache: file content cached in RAM"},
        {"SwapCached",    "Pages swapped out but still in swap cache (fast re-access)"},
        {"Active",        "Pages recently used (unlikely to be reclaimed)"},
        {"Inactive",      "Pages not recently used (candidates for reclaim)"},
        {"SwapTotal",     "Total swap space (disk or zram)"},
        {"SwapFree",      "Unused swap space"},
        {"Dirty",         "Pages modified in page cache not yet written to disk"},
        {"Writeback",     "Pages being written back to disk right now"},
        {"AnonPages",     "Anonymous pages mapped into processes (heap, stack, mmap anon)"},
        {"Mapped",        "Files mapped into memory (mmap'd files, shared libs)"},
        {"Shmem",         "Shared memory (tmpfs, SysV shm, shared anon mmap)"},
        {"KReclaimable",  "Kernel memory that can be reclaimed under pressure"},
        {"Slab",          "Total SLUB/SLAB allocator memory"},
        {"SReclaimable",  "Reclaimable slab (dcache, icache — freed under pressure)"},
        {"SUnreclaim",    "Unreclaimable slab (kernel data structures always needed)"},
        {"PageTables",    "Memory used by page table entries"},
        {"VmallocTotal",  "Total vmalloc virtual address space"},
        {"VmallocUsed",   "Used vmalloc space (kernel modules, ioremap)"},
        {"HugePages_Total","Pre-allocated 2 MiB huge pages"},
        {"AnonHugePages", "Transparent huge pages in use by anonymous regions"},
        {NULL, NULL}
    };

    char line[256];
    while (fgets(line, sizeof line, f)) {
        char key[64];
        if (sscanf(line, "%63[^:]", key) == 1) {
            for (int i = 0; annotations[i].key; i++) {
                if (strcmp(key, annotations[i].key) == 0) {
                    printf("  %-20s  ← %s\n", line + (strlen(key) + 1 > 0 ? strlen(key) + 1 : 0),
                           annotations[i].explanation);
                    /* simpler: just print the raw line + annotation */
                    break;
                }
            }
        }
        /* just print all lines cleanly */
        printf("  %s", line);
    }
    fclose(f);

    printf("\n  QUIZ:\n");
    printf("    Q1. A server shows MemFree=100MB but MemAvailable=4GB. Is it under\n");
    printf("        memory pressure? Why are these two values so different?\n");
    printf("    Q2. What does a large 'Dirty' value indicate and when is it a concern?\n");
    printf("    Q3. Why is SReclaimable memory 'free' for practical purposes?\n");
}

/* ── Exercise 8: Kernel threads ─────────────────────────────────────────── */

static void ex_kthreads(void) {
    section("Exercise 8: Kernel Thread Observation via /proc");

    printf("  WHAT: Kernel threads appear in /proc just like user processes,\n");
    printf("        but have no user-space address space (VmSize=0, mm=NULL).\n");
    printf("  WHY:  kswapd reclaims memory, kcompactd defragments, writeback\n");
    printf("        flushes dirty pages, ksoftirqd runs softirq handlers.\n\n");

    printf("  Key kernel threads (look for these with 'ps aux | grep -E \"kswapd|kcompactd|writeback\"'):\n\n");

    const struct { const char *name; const char *role; } kthreads[] = {
        {"kswapd0",       "Memory reclaim — scans LRU lists, frees pages"},
        {"kcompactd0",    "Memory compaction — moves pages to reduce fragmentation"},
        {"kworker/*",     "Workqueue threads — deferred kernel work items"},
        {"writeback",     "Page cache writeback — flushes dirty pages to disk"},
        {"ksoftirqd/*",   "Softirq handler — network RX, timers, tasklets"},
        {"migration/*",   "CPU migration — moves tasks between CPUs for load balance"},
        {"watchdog/*",    "Hard lockup detector — fires NMI if CPU stuck"},
        {"kthreadd",      "PID 2: parent of all kernel threads"},
        {NULL, NULL}
    };

    for (int i = 0; kthreads[i].name; i++)
        printf("  %-20s  %s\n", kthreads[i].name, kthreads[i].role);

    printf("\n  Checking PID 2 (kthreadd):\n");
    print_proc_file("/proc/2/status", 8);
    printf("  OBSERVE: VmSize and VmRSS are absent — kernel threads have mm=NULL.\n");

    printf("\n  QUIZ:\n");
    printf("    Q1. Why do kernel threads have no virtual address space?\n");
    printf("    Q2. When does kswapd wake up? What triggers it?\n");
    printf("    Q3. What is the difference between kswapd and kcompactd?\n");
    printf("    Q4. Why is there one kswapd per NUMA node?\n");
}

/* ── Exercise 9: Process lineage ─────────────────────────────────────────── */

static void ex_process_lineage(void) {
    section("Exercise 9: Process Lineage — fork -> exec -> wait Chain");

    printf("  WHAT: Demonstrate the complete fork/exec/wait lifecycle and observe\n");
    printf("        how /proc entries appear and disappear.\n");
    printf("  WHY:  Every shell command, every container start, every Kubernetes\n");
    printf("        pod launch uses exactly this sequence.\n\n");

    printf("  Parent PID: %d   PPID: %d\n", getpid(), getppid());

    /* fork a child, have it sleep briefly, observe its /proc entry */
    pid_t child = fork();
    if (child == 0) {
        /* child */
        printf("  Child  PID: %d   PPID: %d\n", getpid(), getppid());
        printf("  Child reading its own /proc/%d/status:\n", getpid());

        char path[64];
        snprintf(path, sizeof path, "/proc/%d/status", getpid());
        FILE *f = fopen(path, "r");
        if (f) {
            char line[256];
            int n = 0;
            while (fgets(line, sizeof line, f) && n++ < 6) printf("    %s", line);
            fclose(f);
        }

        printf("  Child: about to exit(42)\n");
        _exit(42);
    }

    /* parent: briefly observe zombie state before wait() */
    usleep(50000); /* 50 ms — child has likely exited */
    printf("\n  Parent: child %d has exited. Checking for zombie state...\n", child);

    char zpath[64];
    snprintf(zpath, sizeof zpath, "/proc/%d/status", child);
    FILE *zf = fopen(zpath, "r");
    if (zf) {
        char line[256];
        while (fgets(line, sizeof line, zf)) {
            if (strncmp(line, "State", 5) == 0) {
                printf("  %s", line);  /* shows Z (zombie) if caught in time */
                break;
            }
        }
        fclose(zf);
    } else {
        printf("  /proc/%d already gone (may have been reaped by timing)\n", child);
    }

    int status;
    pid_t reaped = waitpid(child, &status, 0);
    printf("  Parent reaped PID %d: exit_status=%d\n", reaped,
           WIFEXITED(status) ? WEXITSTATUS(status) : -1);
    printf("  After wait(): /proc/%d/ no longer exists.\n", child);

    printf("\n  OBSERVE:\n");
    printf("    Between child exit and parent wait(): child is a ZOMBIE (Z state).\n");
    printf("    It consumes a PID slot and a task_struct but no memory.\n");
    printf("    After wait(): task_struct is freed, PID is recycled.\n");

    printf("\n  QUIZ:\n");
    printf("    Q1. What resources does a zombie process hold?\n");
    printf("    Q2. What happens if the parent never calls wait() and itself exits?\n");
    printf("    Q3. How does PID 1 (systemd) avoid zombie accumulation?\n");
    printf("    Q4. Why does exec() reset signal handlers to SIG_DFL?\n");
}

/* ── Exercise 10: cgroup memory limits ─────────────────────────────────── */

static void ex_cgroup_memory(void) {
    section("Exercise 10: cgroup v2 Memory Limit Detection");

    printf("  WHAT: cgroups v2 exposes resource limits as files in /sys/fs/cgroup/.\n");
    printf("  WHY:  Every container runtime (Docker, containerd, CRI-O) and\n");
    printf("        orchestrator (Kubernetes) uses cgroups v2 to enforce limits.\n");
    printf("        Understanding how limits are enforced helps diagnose OOM kills\n");
    printf("        and throttling in production.\n\n");

    const char *cg_paths[] = {
        "/sys/fs/cgroup/memory.max",
        "/sys/fs/cgroup/memory.current",
        "/sys/fs/cgroup/memory.high",
        "/sys/fs/cgroup/cpu.max",
        "/sys/fs/cgroup/cpu.stat",
        "/sys/fs/cgroup/memory.stat",
        NULL
    };

    for (int i = 0; cg_paths[i]; i++) {
        printf("  %-45s : ", cg_paths[i]);
        FILE *f = fopen(cg_paths[i], "r");
        if (!f) {
            printf("(not found — may be running outside a cgroup or on cgroups v1)\n");
        } else {
            char line[256];
            if (fgets(line, sizeof line, f)) printf("%s", line);
            fclose(f);
        }
    }

    printf("\n  Interpreting memory.max:\n");
    printf("    'max'         = no limit (bare metal or unlimited container)\n");
    printf("    '67108864'    = 64 MiB limit (Docker --memory=64m)\n");
    printf("    '1073741824'  = 1 GiB limit  (Kubernetes resources.limits.memory: 1Gi)\n\n");

    printf("  Interpreting cpu.max:\n");
    printf("    'max 100000'  = no CPU limit\n");
    printf("    '200000 100000' = 2.0 cores (Docker --cpus=2)\n\n");

    printf("  QUIZ:\n");
    printf("    Q1. What is the difference between memory.max and memory.high in cgroups v2?\n");
    printf("    Q2. How does the kernel enforce memory.max? What happens when a process\n");
    printf("        exceeds it?\n");
    printf("    Q3. How does Kubernetes translate 'resources.limits.cpu: 500m' into\n");
    printf("        a cgroup cpu.max value?\n");
    printf("    Q4. What is the difference between cgroups v1 and v2 hierarchy?\n");
    printf("        Why does Kubernetes prefer v2?\n");
}

/* ── main ────────────────────────────────────────────────────────────────── */

int main(void) {
    printf("╔══════════════════════════════════════════════════════════════════╗\n");
    printf("║  Process & Memory Subsystems — Deep Dive Exercise               ║\n");
    printf("║  Based on: https://iitd-os.github.io/os-nptel/                 ║\n");
    printf("║            https://www.cse.iitd.ac.in/~sbansal/os/             ║\n");
    printf("╚══════════════════════════════════════════════════════════════════╝\n");
    printf("PID: %d  (use this in /proc/<pid>/ commands while the program runs)\n", getpid());

    /*
     * EXPECTED OUTPUT (Linux x86_64, 8 GiB RAM):
     *
     *   Exercise 1: task_struct fields
     *     VmSize:  12345 kB    VmRSS: 3456 kB    VmPTE: 72 kB
     *
     *   Exercise 3: page fault lifecycle
     *     After mmap(32 MiB):    minor_faults=2
     *     After read scan:       minor_faults=0   (zero page, no alloc)
     *     After memset(32 MiB):  minor_faults=8192 (~32M/4K pages)
     *     After 2nd memset:      minor_faults=0
     *
     *   Exercise 6: huge pages
     *     Random-stride scan 4K pages: ~1.8s
     *     Random-stride scan THP/2M:   ~0.4s   (4.5x speedup)
     */

    ex_task_struct();
    ex_mm_struct();
    ex_page_fault_lifecycle();
    ex_mmap_types();
    ex_oom_killer();
    ex_huge_pages();
    ex_meminfo_tour();
    ex_kthreads();
    ex_process_lineage();
    ex_cgroup_memory();

    printf("\n\n");
    section("Summary: What You Should Now Be Able to Explain");
    printf("  1.  What each field in /proc/<pid>/status maps to in task_struct\n");
    printf("  2.  The sequence: mmap() -> first-touch fault -> PTE install -> warm access\n");
    printf("  3.  Why VmSize >> VmRSS for most processes\n");
    printf("  4.  How MAP_PRIVATE COW differs from MAP_SHARED semantics\n");
    printf("  5.  How the OOM killer scores processes and how Kubernetes adjusts scores\n");
    printf("  6.  Why huge pages reduce TLB misses and when to use madvise(MADV_HUGEPAGE)\n");
    printf("  7.  What MemAvailable means vs MemFree in /proc/meminfo\n");
    printf("  8.  Why kernel threads show VmSize=0 in /proc/<pid>/status\n");
    printf("  9.  The zombie lifecycle and why reaping is critical in PID namespaces\n");
    printf(" 10.  How cgroup v2 memory.max maps to Kubernetes resource limits\n");

    printf("\n  Next steps:\n");
    printf("    Run labs 08, 09, 10, 27, 28 and cross-reference the /proc output\n");
    printf("    Read kernel source: mm/memory.c (page fault handler)\n");
    printf("                        mm/mmap.c   (mmap, VMA management)\n");
    printf("                        mm/vmscan.c (kswapd, LRU reclaim)\n");
    printf("                        kernel/fork.c (do_fork, copy_mm)\n");
    return 0;
}
