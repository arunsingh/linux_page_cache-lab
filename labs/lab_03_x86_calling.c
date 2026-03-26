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

/* WHY: The red zone is a 128-byte area BELOW the current rsp on x86_64.
 *      The ABI guarantees that asynchronous events (signals, interrupts) will NOT
 *      clobber it.  Leaf functions (those that make no further calls) can use it
 *      for locals without adjusting rsp -- saving the sub/add pair in the prologue.
 *      The Linux kernel does NOT use the red zone because interrupts use the same
 *      rsp and would overwrite it.  Kernel code is compiled with -mno-red-zone.
 *
 * WHY: ARM64 ABI (AAPCS64) differs from x86_64 SysV in key ways:
 *      - Integer args: x0-x7 (8 regs vs x86_64's 6)
 *      - Return: x0 (or x0+x1 for 128-bit)
 *      - Frame pointer: x29 (mandatory for unwinding in most OS builds)
 *      - No red zone: sp must always be 16-byte aligned (strict alignment fault)
 *      - Link register: x30 holds return address (not pushed to stack by CALL)
 *      Apple Silicon (M1/M2/M3) and AWS Graviton use AAPCS64.
 */

/* OBSERVE: This function is ideal for disassembly with objdump.
 *          It uses the first two arg registers (rdi, rsi on x86_64; x0, x1 on ARM64)
 *          and returns via rax / x0 respectively. */
__attribute__((noinline))
static long add_two(long a, long b) {
    return a + b;
}

static void demo_disassembly_hint(void) {
    print_section("Phase 5: Disassembly Verification Exercise");

    long result = add_two(17, 25);
    printf("  add_two(17, 25) = %ld\n", result);
    printf("  Function pointer: %p\n", (void *)(uintptr_t)add_two);

    printf("\n  x86_64 objdump command:\n");
    printf("    objdump -d lab_03 | awk '/^[0-9a-f]+ <add_two>/,/^$/' | head -20\n");
    printf("  Expected: movq %%rdi, %%rax; addq %%rsi, %%rax; retq\n");
    printf("            (Compiler puts 'a' in rdi, 'b' in rsi, result in rax)\n\n");

    printf("  ARM64 objdump command (cross-compiled or on Apple Silicon):\n");
    printf("    objdump -d lab_03 | awk '/^[0-9a-f]+ <add_two>/,/^$/' | head -20\n");
    printf("  Expected: add x0, x0, x1; ret\n");
    printf("            (ARM64: x0=a, x1=b, result in x0, return via lr/x30)\n\n");

    /* OBSERVE: At -O0, the compiler stores args to the stack first.
     *          At -O2, the function is typically 2-3 instructions.
     *          The ABI is visible even at -O0 in the prologue register saves. */
    printf("  OBSERVE: Compile with -O0 to see full frame, -O2 to see ABI-only form.\n");
    printf("  WHY: The ABI defines which registers are caller-saved (must be preserved\n");
    printf("       by caller) vs callee-saved (must be preserved by callee):\n");
    printf("    x86_64 callee-saved: rbx, rbp, r12-r15\n");
    printf("    ARM64  callee-saved: x19-x28, x29 (fp), x30 (lr)\n");
}

int main(void) {
    /* EXPECTED OUTPUT (Linux x86_64):
     *   Phase 1: a(rdi)=1 b(rsi)=2 c(rdx)=3 d(rcx)=4 e(r8)=5 f(r9)=6
     *            g(stack)=7 h(stack)=8  Return value (rax)=36
     *   Phase 2: 4 stack frames, each with lower rsp than parent (stack grows down)
     *            frame_size typically 16-48 bytes (alignment + locals)
     *   Phase 3: TSC value (large uint64), RDTSC back-to-back overhead ~20-40 cycles
     *   Phase 4: without/with volatile both return 499500 (sum 0..999)
     *   Phase 5: add_two(17,25)=42; disassembly tip printed
     */
    printf("=== Lab 03: x86 Instruction Set, GCC Calling Conventions ===\n");

    demo_calling_convention();
    demo_stack_layout();
    demo_inline_asm();
    demo_volatile_effect();
    demo_disassembly_hint();

    /* === EXERCISE ===
     * Try these hands-on tasks:
     * 1. Compile this file and disassemble add_two() with both -O0 and -O2:
     *      gcc -O0 -Wall -o lab_03_O0 lab_03_x86_calling.c
     *      gcc -O2 -Wall -o lab_03_O2 lab_03_x86_calling.c
     *      objdump -d lab_03_O0 | awk '/add_two/,/^$/' | head -20
     *    Count the instructions: -O2 should produce 2-3 instructions vs ~15 for -O0.
     * 2. Add a function with 7 integer arguments and disassemble it.
     *    Find where the 7th argument is loaded in the caller (hint: look for push or
     *    movq to (%rsp) just before the call instruction).  Verify it is on the stack.
     * 3. Modern (ARM64 ABI differences): if you have access to an Apple Silicon Mac
     *    or AWS Graviton, cross-compile with 'aarch64-linux-gnu-gcc' and disassemble.
     *    Compare: x86_64 uses 6 arg regs (rdi,rsi,...,r9); ARM64 uses 8 (x0-x7).
     *    Also verify: ARM64 has NO red zone -- sp must be 16-byte aligned at all times.
     *
     * OBSERVE: At -O2, add_two() becomes 2 instructions: add + ret on x86_64.
     *          At -O0, there is a full prologue (push rbp; mov rsp,rbp) and
     *          epilogue (pop rbp; ret) even for a trivial function.
     * WHY:     The red zone means leaf functions at -O2 can skip the sub rsp, N
     *          stack allocation step, using the 128-byte zone below rsp for spills.
     *          This is why kernel code uses -mno-red-zone: IRQ handlers would corrupt it.
     */
    printf("\n========== Hands-On Exercise ==========\n");
    printf("1. Compile at -O0 and -O2; disassemble add_two() and count instructions.\n");
    printf("2. Write a 7-arg function; find the 7th arg loaded to stack in the caller.\n");
    printf("3. Modern (ARM64): cross-compile and compare x0-x7 register args vs\n");
    printf("   x86_64 rdi-r9; verify ARM64 has no red zone (sp must stay 16B aligned).\n");

    printf("\n========== Quiz ==========\n");
    printf("Q1. How many integer arguments fit in registers on x86_64? Where do the rest go?\n");
    printf("Q2. Draw the stack layout for a function with 2 local variables and 1 argument on stack.\n");
    printf("Q3. What is the 'red zone' on x86_64 and why does the kernel not use it?\n");
    printf("Q4. Why must device driver code use volatile for MMIO accesses?\n");
    printf("Q5. What does RDTSC measure and why is it useful for micro-benchmarking?\n");
    printf("Q6. How does the ARM64 (AAPCS64) calling convention differ from x86_64 SysV ABI\n");
    printf("    in terms of: number of arg regs, return register, frame pointer, red zone?\n");
    printf("Q7. What are callee-saved vs caller-saved registers, and why does the distinction\n");
    printf("    matter when writing an interrupt handler or signal handler in assembly?\n");
    return 0;
}
