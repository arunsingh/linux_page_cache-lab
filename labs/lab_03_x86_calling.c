/*
 * lab_03_x86_calling.c
 *
 * Topic: x86 Instruction Set, GCC Calling Conventions
 *
 * Demonstrates:
 *   1. System V AMD64 ABI calling convention (rdi, rsi, rdx, rcx, r8, r9)
 *   2. Stack frame layout inspection
 *   3. Inline assembly basics
 *   4. Red zone demonstration
 *   5. Function prologue/epilogue via objdump
 *
 * Build: gcc -O0 -Wall -o lab_03 lab_03_x86_calling.c
 *        gcc -S -O0 -o lab_03.s lab_03_x86_calling.c   # generate assembly
 * Run:   ./lab_03
 * Also:  objdump -d lab_03 | grep -A 20 '<demo_stack_frame>'
 *
 * Author: Arun Singh | arunsingh.in@gmail.com
 * OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
*/

#include <stdio.h>
#include <stdint.h>
#include <string.h>

static void print_section(const char *title) {
    printf("\n========== %s ==========\n", title);
}

/* --- Phase 1: Calling convention demonstration --- */

/* This function takes 8 args to show register vs stack passing */
__attribute__((noinline))
static long demo_args(long a, long b, long c, long d, long e, long f, long g, long h) {
    printf("  a(rdi)=%ld b(rsi)=%ld c(rdx)=%ld d(rcx)=%ld e(r8)=%ld f(r9)=%ld\n",
           a, b, c, d, e, f);
    printf("  g(stack)=%ld h(stack)=%ld\n", g, h);
    return a + b + c + d + e + f + g + h;
}

static void demo_calling_convention(void) {
    print_section("Phase 1: System V AMD64 Calling Convention");
    printf("  First 6 integer args go in registers: rdi, rsi, rdx, rcx, r8, r9\n");
    printf("  Additional args are pushed onto the stack right-to-left.\n");
    printf("  Return value goes in rax.\n\n");

    long result = demo_args(1, 2, 3, 4, 5, 6, 7, 8);
    printf("  Return value (rax): %ld\n", result);
}

/* --- Phase 2: Stack frame inspection --- */

__attribute__((noinline))
static void demo_stack_frame(int depth) {
    int local_var = 0xDEAD + depth;
    void *frame_ptr;
    void *stack_ptr;
    void *ret_addr;

#if defined(__x86_64__)
    __asm__ volatile("mov %%rbp, %0" : "=r"(frame_ptr));
    __asm__ volatile("mov %%rsp, %0" : "=r"(stack_ptr));
    /* Return address is at [rbp+8] */
    ret_addr = *(void **)((char *)frame_ptr + 8);
#else
    frame_ptr = stack_ptr = ret_addr = NULL;
#endif

    printf("  depth=%d  &local_var=%p  value=0x%x\n", depth, (void *)&local_var, local_var);
    printf("           rbp=%p  rsp=%p  ret_addr=%p\n", frame_ptr, stack_ptr, ret_addr);
    printf("           frame_size=%ld bytes\n",
           (long)((char *)frame_ptr - (char *)stack_ptr));

    if (depth > 0) {
        demo_stack_frame(depth - 1);
    }
}

static void demo_stack_layout(void) {
    print_section("Phase 2: Stack Frame Layout");
    printf("  Each function call creates a stack frame: [ret_addr][saved_rbp][locals]\n");
    printf("  Stack grows DOWNWARD (higher addresses = earlier frames).\n\n");

    demo_stack_frame(3);

    printf("\n  OBSERVE: Each recursive call has a lower rsp (stack grew down).\n");
    printf("           rbp points to the previous frame's rbp (linked list of frames).\n");
    printf("  TIP: Run 'objdump -d lab_03 | grep -A 20 demo_stack_frame' to see\n");
    printf("       push %%rbp / mov %%rsp,%%rbp (prologue) and pop %%rbp / ret (epilogue).\n");
}

/* --- Phase 3: Inline assembly --- */

static void demo_inline_asm(void) {
    print_section("Phase 3: Inline Assembly Basics");

#if defined(__x86_64__)
    /* Read timestamp counter */
    uint64_t tsc;
    __asm__ volatile("rdtsc; shlq $32, %%rdx; orq %%rdx, %%rax"
                     : "=a"(tsc) : : "rdx");
    printf("  TSC (Time Stamp Counter): %lu\n", tsc);

    /* Read CR3 would require ring 0, so we skip it */
    printf("  Note: CR3 (page table base) is ring-0 only — cannot read from userspace.\n");

    /* CPUID for demonstration */
    unsigned eax, ebx, ecx, edx;
    __asm__ volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0));
    printf("  CPUID leaf 0: eax=%u (max leaf)\n", eax);

    /* Measure RDTSC overhead */
    uint64_t t0, t1;
    unsigned lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    t0 = ((uint64_t)hi << 32) | lo;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    t1 = ((uint64_t)hi << 32) | lo;
    printf("  RDTSC back-to-back overhead: ~%lu cycles\n", t1 - t0);
#else
    printf("  Inline assembly examples are x86_64-specific.\n");
#endif
}

/* --- Phase 4: Volatile and optimization --- */

__attribute__((noinline))
static int compute_without_volatile(int n) {
    int sum = 0;
    for (int i = 0; i < n; i++) sum += i;
    return sum;
}

__attribute__((noinline))
static int compute_with_volatile(int n) {
    volatile int sum = 0;
    for (int i = 0; i < n; i++) sum += i;
    return sum;
}

static void demo_volatile_effect(void) {
    print_section("Phase 4: Volatile and Compiler Optimization");
    printf("  'volatile' tells the compiler not to optimize away memory accesses.\n");
    printf("  This matters for memory-mapped I/O and shared memory.\n\n");

    int r1 = compute_without_volatile(1000);
    int r2 = compute_with_volatile(1000);
    printf("  Without volatile: result=%d\n", r1);
    printf("  With volatile:    result=%d\n", r2);
    printf("  Both produce the same result, but volatile version forces every\n");
    printf("  store/load to actually hit memory, not just live in a register.\n");
    printf("  Compile with -O2 and check assembly: gcc -O2 -S lab_03_x86_calling.c\n");
}

int main(void) {
    printf("=== Lab 03: x86 Instruction Set, GCC Calling Conventions ===\n");

    demo_calling_convention();
    demo_stack_layout();
    demo_inline_asm();
    demo_volatile_effect();

    printf("\n========== Quiz ==========\n");
    printf("Q1. How many integer arguments fit in registers on x86_64? Where do the rest go?\n");
    printf("Q2. Draw the stack layout for a function with 2 local variables and 1 argument on stack.\n");
    printf("Q3. What is the 'red zone' on x86_64 and why does the kernel not use it?\n");
    printf("Q4. Why must device driver code use volatile for MMIO accesses?\n");
    printf("Q5. What does RDTSC measure and why is it useful for micro-benchmarking?\n");
    return 0;
}
