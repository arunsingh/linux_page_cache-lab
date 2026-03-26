/*
 * lab_34_protection.c
 * Topic: Protection and Security
 * Build: gcc -O0 -Wall -pthread -o lab_34 lab_34_protection.c
 *
 * Author: Arun Singh | arunsingh.in@gmail.com
 * OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
*/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/prctl.h>
/* sys/capability.h — install libcap-dev if available */
#include <linux/seccomp.h>
#include <sys/syscall.h>
#include <sched.h>
static void ps(const char *t){printf("\n========== %s ==========\n",t);}
int main(void){
    printf("=== Lab 34: Protection and Security ===\n");
    ps("Phase 1: Linux Capabilities");
    printf("  Traditional: root (UID 0) can do everything.\n");
    printf("  Capabilities: fine-grained privileges (CAP_NET_ADMIN, CAP_SYS_PTRACE, etc.)\n\n");
    printf("  This process (PID %d) capabilities:\n",getpid());
    char path[64];snprintf(path,sizeof(path),"/proc/%d/status",getpid());
    FILE *fp=fopen(path,"r");
    if(fp){char l[256];while(fgets(l,sizeof(l),fp))if(strncmp(l,"Cap",3)==0)printf("  %s",l);fclose(fp);}
    ps("Phase 2: Namespaces");
    printf("  Linux namespaces isolate system resources per process group:\n");
    printf("    PID ns:  separate PID numbering (containers see PID 1)\n");
    printf("    NET ns:  separate network stack\n");
    printf("    MNT ns:  separate mount table\n");
    printf("    UTS ns:  separate hostname\n");
    printf("    USER ns: separate UID/GID mapping\n");
    printf("    IPC ns:  separate IPC resources\n");
    printf("  Docker/containers use ALL of these together.\n");
    ps("Phase 3: Seccomp");
    printf("  Seccomp filters restrict which syscalls a process can make.\n");
    printf("  Used by: Chrome, Docker, Android, systemd.\n");
    printf("  Mode 1: only read/write/exit/sigreturn.\n");
    printf("  Mode 2 (BPF): custom filter program per syscall.\n");
    ps("Phase 4: ASLR");
    printf("  Address Space Layout Randomization:\n");
    void *stack_var;
    printf("    Stack address:  %p (changes each run)\n",(void*)&stack_var);
    printf("    Heap address:   %p\n",malloc(1));
    printf("    Code address:   %p\n",(void*)main);
    printf("  ASLR makes exploits harder by randomizing memory layout.\n");
    /* WHY: eBPF for security policy enforcement (Linux 5.x+):
     *      eBPF programs can be attached to LSM (Linux Security Module) hooks via
     *      BPF_LSM programs.  This allows implementing security policies (like SELinux
     *      or AppArmor) in BPF, which are:
     *      - Hot-reloadable (no kernel recompile or reboot)
     *      - Auditable (verifier ensures they terminate and don't crash the kernel)
     *      - Composable (multiple LSM hooks can stack)
     *      Cilium uses eBPF for network security policy in Kubernetes.
     *      Falco uses eBPF for runtime threat detection in containers.
     *
     * WHY: DAC (Discretionary Access Control) = traditional Unix permissions (owner sets).
     *      MAC (Mandatory Access Control) = SELinux/AppArmor (system policy overrides owner).
     *      Example: a file owned by root with permissions 644 can be read by anyone (DAC).
     *      With SELinux: even root cannot read a file if the SELinux policy denies it.
     *      Containers: MAC provides defense-in-depth when a container is compromised.
     */
    ps("Phase 5: DAC vs MAC, eBPF Security");
    printf("  DAC (Discretionary Access Control) -- traditional Unix:\n");
    printf("    - Owner of file sets permissions (rwxrwxrwx)\n");
    printf("    - Root (UID 0) can override any DAC check\n");
    printf("    - Process runs as UID and inherits its permissions\n\n");
    printf("  MAC (Mandatory Access Control) -- SELinux / AppArmor:\n");
    printf("    - System policy (not owner) enforces access\n");
    printf("    - Even root is subject to MAC policy\n");
    printf("    - SELinux labels: user:role:type:level on files and processes\n");
    printf("    - AppArmor profiles: path-based rules per process\n\n");

    /* Check SELinux status */
    FILE *fp2 = fopen("/sys/fs/selinux/enforce", "r");
    if (fp2) {
        char val[4] = {0};
        if (fgets(val, sizeof(val), fp2)) {
            val[strcspn(val,"\n")] = '\0';
            printf("  SELinux enforcing: %s\n", atoi(val) ? "YES" : "NO (permissive/disabled)");
        }
        fclose(fp2);
    } else {
        printf("  SELinux: not present (check AppArmor: cat /sys/kernel/security/apparmor/profiles)\n");
    }

    printf("\n  eBPF LSM (Linux 5.7+):\n");
    printf("    - BPF programs attached to LSM hooks (security_file_open, etc.)\n");
    printf("    - Allows custom security policies without kernel module\n");
    printf("    - Cilium: eBPF network policy for Kubernetes pod-to-pod security\n");
    printf("    - Falco: eBPF syscall monitoring for runtime threat detection\n");
    printf("    - Check: bpftool prog list (shows loaded BPF programs on this system)\n");

    /* === EXERCISE ===
     * Try these hands-on tasks:
     * 1. Demonstrate DAC: create a file with chmod 000 (no permissions).
     *    Try to read it as regular user (fails).  Try as root (succeeds -- DAC bypass).
     *    Then on an SELinux system, create a type transition rule that prevents root
     *    from reading a specific file type.  This shows MAC overriding DAC.
     * 2. Demonstrate capabilities: use capsh --print to show current capabilities.
     *    Use setcap cap_net_raw+ep /tmp/testprog to add a single capability.
     *    Verify: /tmp/testprog can open raw sockets without root (CAP_NET_RAW).
     *    This shows the least-privilege principle: give only needed capabilities.
     * 3. Modern (eBPF security): install bpftool and list all loaded BPF programs:
     *      bpftool prog list
     *    On a Kubernetes node with Cilium: dozens of BPF programs enforce network policy.
     *    On a system with Falco: BPF programs monitor sys_open, sys_connect, etc.
     *    Explain: how does BPF LSM differ from seccomp-bpf in terms of what it can
     *    enforce (syscall filter vs security hook callback)?
     *
     * OBSERVE: ASLR randomizes only the offset within each segment, not the segment order.
     *          On 64-bit: 28-bit entropy for mmap (256TB range / 4KB pages = 2^36, but
     *          only lower bits used).  With info leaks (format string vuln), ASLR can be
     *          defeated by reading one address and computing others relatively.
     * WHY:     Capabilities let containers drop all capabilities except needed ones.
     *          Docker by default drops: CAP_SYS_ADMIN, CAP_NET_ADMIN, CAP_SYS_PTRACE.
     *          This prevents container escapes via kernel attack surfaces.
     */
    printf("\n========== Hands-On Exercise ==========\n");
    printf("1. Create chmod 000 file; verify root bypasses DAC; test SELinux MAC overrides root.\n");
    printf("2. Use setcap to grant cap_net_raw+ep without root; verify minimal privilege.\n");
    printf("3. Modern (eBPF LSM): run 'bpftool prog list'; on a Cilium/Falco node,\n");
    printf("   identify security BPF programs; explain BPF LSM vs seccomp-bpf scope.\n");

    printf("\n========== Quiz ==========\n");
    printf("Q1. Why are capabilities better than all-or-nothing root?\n");
    printf("Q2. How do PID namespaces make containers think they have PID 1?\n");
    printf("Q3. What is seccomp-bpf and how does Docker use it?\n");
    printf("Q4. Can ASLR be bypassed? How?\n");
    printf("Q5. What is the difference between DAC and MAC?  Give an example where\n");
    printf("    MAC prevents access that DAC would allow (even for root).\n");
    printf("Q6. How does eBPF LSM differ from traditional SELinux in terms of policy\n");
    printf("    deployment and maintainability?\n");
    printf("Q7. What capabilities does Docker drop by default from containers, and\n");
    printf("    why does dropping CAP_SYS_ADMIN prevent certain container escapes?\n");
    return 0;
}
