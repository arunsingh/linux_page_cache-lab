/*
 * lab_13.c
 * Topic: Setting up page tables for user processes
 * Build: gcc -O0 -Wall -pthread -o lab_13 lab_13_*.c
 *
 * Author: Arun Singh | arunsingh.in@gmail.com
 * OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/resource.h>
static void print_section(const char *t){printf("\n========== %s ==========\n",t);}
static void get_faults(long *mi,long *ma){struct rusage r;getrusage(RUSAGE_SELF,&r);*mi=r.ru_minflt;*ma=r.ru_majflt;}

/* WHY: KPTI (Kernel Page Table Isolation) gives each process TWO sets of page tables:
 *      1. User-mode tables: only user-space mappings (no kernel mappings visible).
 *      2. Kernel-mode tables: full mapping (user + kernel linear map + modules).
 *      On syscall entry, CR3 switches to the kernel table.  On return, switches back.
 *      This means the kernel is invisible from user space -- Meltdown cannot read it.
 *      PCID (Process Context Identifier) tags TLB entries so the switch does not
 *      always require a full TLB flush (only if the PCID slot has been reused). */

int main(void){
    printf("=== Lab 13: Setting Up Page Tables for User Processes ===\n");

    /* EXPECTED OUTPUT (Linux x86_64):
     *   Phase 1: Child faults after fork (before write): minor=0 or very small
     *            Child faults after writing 32 MiB: minor=~8192 (COW copies)
     *            Parent: mem[0]=0xAA (unchanged -- COW isolation)
     *   Phase 2: execve() page table replacement described
     *   Phase 3: meltdown mitigation file content; KPTI status
     *   Phase 4: parent and child VMA counts match immediately after fork()
     */

    print_section("Phase 1: Fork and COW Page Table Copy");
    long m0,j0,m1,j1;
    size_t sz=32*1024*1024;
    char *mem=mmap(NULL,sz,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    memset(mem,0xAA,sz);
    printf("  Parent: allocated and touched %zu MiB\n",sz/(1024*1024));
    /* OBSERVE: fork() duplicates page tables with W=0 on all writable PTEs.
     *          No physical pages are copied -- just page table entries modified.
     *          Cost is O(number of PTEs), not O(physical pages). */
    get_faults(&m0,&j0);
    pid_t pid=fork();
    if(pid==0){
        get_faults(&m1,&j1);
        printf("  Child: faults after fork (before write): minor=%ld\n",m1-m0);
        get_faults(&m0,&j0);
        /* WHY: Each write to a shared COW page triggers a #PF (W=0 PTE).
         *      Kernel allocates new page, copies content, remaps with W=1.
         *      This is the COW "tax" paid at first write. */
        memset(mem,0xBB,sz);
        get_faults(&m1,&j1);
        printf("  Child: faults after writing %zu MiB: minor=%ld (COW copies)\n",sz/(1024*1024),m1-m0);
        _exit(0);
    }
    wait(NULL);
    printf("  Parent: mem[0]=0x%02x (unchanged by child COW)\n",(unsigned char)mem[0]);
    munmap(mem,sz);

    /* OBSERVE: execve() discards entire mm_struct and creates a fresh one for the
     *          new program.  The new page tables map only the new executable's
     *          segments (text, data, BSS, stack, vdso, vvar). */
    print_section("Phase 2: execve() Replaces Page Tables");
    printf("  When execve() is called, the kernel:\n");
    printf("  1. Releases the old mm_struct and all its VMAs\n");
    printf("  2. Creates a new mm_struct with new page tables\n");
    printf("  3. Maps the new executable: code, data, BSS, stack\n");
    printf("  4. Sets up the initial stack with argc, argv, envp\n");
    printf("  This is why fork()+execve() is cheap: fork COWs, execve replaces.\n");

    print_section("Phase 3: KPTI Page Table Isolation (Meltdown Mitigation)");
    printf("  KPTI status:\n");
    FILE *fp2 = fopen("/sys/devices/system/cpu/vulnerabilities/meltdown", "r");
    if (fp2) {
        char val[128] = {0};
        if (fgets(val, sizeof(val), fp2)) printf("    meltdown: %s", val);
        fclose(fp2);
    } else {
        printf("    /sys/devices/system/cpu/vulnerabilities/meltdown: not found\n");
    }
    printf("  If 'Mitigation: PTI': user-mode page tables lack kernel mappings.\n");
    printf("  Effect: every syscall costs extra CR3 switch (~100-200 cycles on x86).\n");
    printf("  PCID support reduces this: TLB entries tagged, no full flush needed.\n");

    /* OBSERVE: Fork then compare parent/child VMA counts */
    print_section("Phase 4: Fork and Compare Parent/Child VMA Counts");
    {
        int parent_vmas = 0;
        FILE *fp = fopen("/proc/self/maps", "r");
        char line[512];
        if (fp) {
            while (fgets(line, sizeof(line), fp)) parent_vmas++;
            fclose(fp);
        }
        printf("  Parent VMA count: %d\n", parent_vmas);

        pid_t child = fork();
        if (child == 0) {
            int child_vmas = 0;
            fp = fopen("/proc/self/maps", "r");
            if (fp) {
                while (fgets(line, sizeof(line), fp)) child_vmas++;
                fclose(fp);
            }
            printf("  Child  VMA count: %d (should match parent: COW copy, same layout)\n", child_vmas);
            _exit(0);
        }
        wait(NULL);
        printf("  OBSERVE: Child gets same VMA count and layout as parent immediately after fork.\n");
        printf("  WHY: copy_mm() duplicates the mm_struct and VMA tree (COW page tables).\n");
        printf("       Physical pages are NOT copied; only PTEs set W=0 for COW.\n");
    }

    /* === EXERCISE ===
     * Try these hands-on tasks:
     * 1. Fork a child, and in the child immediately read /proc/self/maps to a buffer.
     *    In the parent, read /proc/<child_pid>/maps using the child's PID.
     *    Compare: they should be identical immediately after fork.  Then have the child
     *    call malloc(1<<20) and re-compare.  The child's [heap] end address grows.
     * 2. Measure the COW copy cost: allocate 256 MiB, touch all pages, then fork().
     *    The child writes to every page -- causing COW copies.
     *    Use getrusage() to count minor faults in the child.  This is the COW "tax".
     * 3. Modern (KPTI page table isolation): check if KPTI is active via
     *      cat /sys/devices/system/cpu/vulnerabilities/meltdown
     *    If 'Mitigation: PTI', use strace to measure syscall latency.  The CR3 switch
     *    overhead is visible as extra ~100-200 cycles in syscall-heavy code.
     *    With PCID support, the overhead is lower (TLB entries tagged, no full flush).
     *
     * OBSERVE: Immediately after fork(), parent and child have identical VMA counts
     *          and identical page table structures.  Physical pages are shared with
     *          W=0 in PTEs.  The first write to any shared page triggers COW:
     *          kernel allocates a new physical page, copies content, remaps with W=1.
     * WHY:     copy_mm() in the kernel does a full VMA tree duplication but only
     *          marks PTEs read-only rather than allocating new physical pages.
     *          This makes fork() cost O(VMAs), not O(physical_pages).
     */
    printf("\n========== Hands-On Exercise ==========\n");
    printf("1. Fork child, read /proc/<child>/maps; compare to parent maps immediately.\n");
    printf("   Have child malloc and re-compare; observe heap growth in child only.\n");
    printf("2. Allocate 256 MiB, touch all, fork, have child write all -- measure\n");
    printf("   COW fault count via getrusage() minflt in the child.\n");
    printf("3. Modern (KPTI): check meltdown vulnerability file; observe syscall cost\n");
    printf("   increase with PTI active vs 'nopti' kernel parameter.\n");

    printf("\n========== Quiz ==========\n");
    printf("Q1. How many page faults does fork() itself cause? Why is it fast?\n");
    printf("Q2. When does the child actually get its own physical pages?\n");
    printf("Q3. What happens to the parent page tables when execve() is called in the child?\n");
    printf("Q4. Why is vfork() sometimes faster than fork() for execve()-only children?\n");
    printf("Q5. What is KPTI and why does it require two separate page table roots per process?\n");
    printf("Q6. How does PCID (Process Context ID) reduce the TLB flush cost of KPTI?\n");
    printf("Q7. After fork(), how does the kernel set up COW page tables for 256 MiB\n");
    printf("    of already-resident pages -- what is the time complexity O(?)?\n");
    return 0;
}
