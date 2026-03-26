/*
 * lab_17_first_process.c
 * Topic: Creating the first process (init/systemd, PID 1)
 * Build: gcc -O0 -Wall -o lab_17 lab_17_first_process.c
 *
 * Author: Arun Singh | arunsingh.in@gmail.com
 * OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

static void print_section(const char *t){printf("\n========== %s ==========\n",t);}

/* WHY: PID 1 (init/systemd) is the first userspace process, created by the kernel
 *      directly from kernel_init() thread.  It cannot be killed (SIGKILL to PID 1
 *      is silently ignored by the kernel).  All other processes are descendants.
 *      Systemd (the modern init) is PID 1 in virtually all major Linux distros.
 *      It reads unit files from /etc/systemd/ and /lib/systemd/system/ to decide
 *      what to start.  In containers, PID 1 is often tini or the app itself.
 *
 * WHY: systemd as PID 1 handles:
 *      - Reaping adopted orphan processes (it is a subreaper for the whole system).
 *      - Socket activation: systemd creates the listening socket, passes fd to service.
 *      - cgroup lifecycle: each service gets its own cgroup slice.
 *      - Journal (systemd-journald): structured logging via /dev/log or sd_journal_print().
 */

static void demo_pid1_info(void) {
    print_section("Phase 1: PID 1 Information (/proc/1/)");

    /* Read PID 1's cmdline */
    printf("  /proc/1/cmdline (init process command):\n    ");
    FILE *fp = fopen("/proc/1/cmdline", "r");
    if (fp) {
        char buf[512] = {0};
        size_t n = fread(buf, 1, sizeof(buf)-1, fp);
        fclose(fp);
        /* cmdline is NUL-separated, print as space-separated */
        for (size_t i = 0; i < n; i++)
            putchar(buf[i] ? buf[i] : ' ');
        putchar('\n');
    } else {
        printf("  (permission denied -- try: cat /proc/1/cmdline)\n");
    }

    /* Read PID 1's status */
    fp = fopen("/proc/1/status", "r");
    if (fp) {
        char line[256];
        printf("\n  /proc/1/status key fields:\n");
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line,"Name:",5)==0 || strncmp(line,"Pid:",4)==0 ||
                strncmp(line,"PPid:",5)==0 || strncmp(line,"State:",6)==0 ||
                strncmp(line,"Threads:",8)==0 || strncmp(line,"VmRSS:",6)==0)
                printf("    %s", line);
        }
        fclose(fp);
    } else {
        printf("  /proc/1/status: permission denied (PID 1 owned by root)\n");
    }

    /* OBSERVE: PPid of PID 1 is 0 (kernel swapper/idle thread, not a real process).
     *          In a container, PID 1 is the first process started by the OCI runtime. */
    printf("\n  OBSERVE: PID 1's PPID=0 (kernel -- the swapper/idle thread).\n");
    printf("           In containers: PID 1 is tini, dumb-init, or the app itself.\n");
    printf("           Container PID 1 that does not reap zombies causes zombie buildup.\n");
}

static void demo_our_ancestors(void) {
    print_section("Phase 2: Walking Our Process Ancestry");

    pid_t pid = getpid();
    printf("  Process ancestry chain (this process -> ... -> PID 1):\n");

    int depth = 0;
    while (pid > 1 && depth < 20) {
        char path[64], line[256];
        pid_t ppid = 0;
        char name[64] = "?";

        snprintf(path, sizeof(path), "/proc/%d/status", pid);
        FILE *fp = fopen(path, "r");
        if (!fp) break;
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line,"Name:",5)==0) sscanf(line+5," %63s",name);
            if (strncmp(line,"PPid:",5)==0) sscanf(line+5," %d",&ppid);
        }
        fclose(fp);

        printf("  %*sPID %d (%s)\n", depth*2, "", pid, name);
        pid = ppid;
        depth++;
    }
    if (pid <= 1)
        printf("  %*sPID 1 (init/systemd)\n", depth*2, "");
}

static void demo_systemd_units(void) {
    print_section("Phase 3: systemd / init Information");

    /* Detect init system */
    struct stat st;
    if (stat("/run/systemd/private", &st) == 0 || stat("/run/systemd", &st) == 0) {
        printf("  Init system: systemd detected\n");
        printf("  systemd is PID 1 on virtually all major Linux distros (2024).\n");
        printf("  Key features:\n");
        printf("    - Unit files: /lib/systemd/system/*.service, *.socket, *.target\n");
        printf("    - Socket activation: fd passed to service via $LISTEN_FDS env var\n");
        printf("    - cgroup-v2: each service in /sys/fs/cgroup/<slice>/<service>/\n");
        printf("    - Journal: structured logs via 'journalctl -u <service>'\n");
    } else {
        printf("  Init system: not systemd (SysVinit, OpenRC, or container runtime)\n");
    }

    /* Check if we are in a container (look for .dockerenv or cgroup namespace) */
    if (stat("/.dockerenv", &st) == 0) {
        printf("\n  Running INSIDE a Docker container.\n");
        printf("  PID namespace: this process sees PID 1 as the container entrypoint.\n");
        printf("  Host PID 1 (systemd) is invisible from inside the container.\n");
    }

    /* Show /proc/1/cgroup to understand cgroup hierarchy */
    FILE *fp = fopen("/proc/1/cgroup", "r");
    if (fp) {
        char line[256];
        printf("\n  /proc/1/cgroup (cgroup membership of PID 1):\n");
        int n = 0;
        while (fgets(line, sizeof(line), fp) && n++ < 5) printf("    %s", line);
        fclose(fp);
    }
}

int main(void){
    /* EXPECTED OUTPUT (Linux x86_64 with systemd):
     *   Phase 1: /proc/1/cmdline: /lib/systemd/systemd or /sbin/init
     *            Name: systemd, Pid: 1, PPid: 0, State: S, Threads: 1
     *   Phase 2: Ancestry: lab_17 -> bash -> sshd (or terminal) -> ... -> systemd (PID 1)
     *   Phase 3: systemd detected; cgroup hierarchy for PID 1 shown
     *   Inside Docker: /proc/1/cmdline is the container entrypoint
     */
    printf("=== Lab 17: Creating the First Process (PID 1, init, systemd) ===\n");
    printf("  This process: PID=%d, PPID=%d\n", getpid(), getppid());

    demo_pid1_info();
    demo_our_ancestors();
    demo_systemd_units();

    /* === EXERCISE ===
     * Try these hands-on tasks:
     * 1. Read /proc/1/cmdline and /proc/1/status.  Compare inside and outside
     *    a Docker container.  Inside: PID 1 is the container entrypoint.
     *    Outside: PID 1 is systemd.  This is the PID namespace isolation.
     * 2. Walk /proc/1/fd/ (requires root) to see which file descriptors PID 1 has open.
     *    systemd holds open socket fds for socket-activated services (e.g., sshd).
     *    Compare the count to a minimal container's PID 1 (often just stdin/stdout/stderr).
     * 3. Modern (systemd in all major distros): understand the boot target chain:
     *    sysinit.target -> basic.target -> multi-user.target -> graphical.target.
     *    Run: systemctl list-units --type=target --state=active
     *    On a GPU server, custom targets start CUDA drivers and NCCL services.
     *    prctl(PR_SET_CHILD_SUBREAPER, 1) lets any process act like init (reap orphans).
     *
     * OBSERVE: PID 1 cannot be killed with SIGKILL -- the kernel ignores it.
     *          If PID 1 exits, the kernel panics: "Attempted to kill init!"
     *          In containers, if PID 1 exits, the container runtime kills all other
     *          processes in the container (all share the same PID namespace).
     * WHY:     The kernel special-cases PID 1 for signal delivery: SIGKILL and SIGTERM
     *          are only delivered if PID 1 has explicitly installed a handler for them.
     *          This prevents accidental system shutdown from a rogue process.
     */
    printf("\n========== Hands-On Exercise ==========\n");
    printf("1. Compare /proc/1/cmdline inside vs outside a Docker container;\n");
    printf("   observe PID namespace: inside, PID 1 is the container entrypoint.\n");
    printf("2. Walk /proc/1/fd/ (as root): count open fds; find socket-activated services.\n");
    printf("3. Modern (systemd): run 'systemctl list-units --type=target --state=active';\n");
    printf("   trace boot target chain; explain prctl(PR_SET_CHILD_SUBREAPER) usage.\n");

    printf("\n========== Quiz ==========\n");
    printf("Q1. How is PID 1 created -- which kernel function spawns the first userspace process?\n");
    printf("Q2. What happens if PID 1 exits? Why is this a special case in the kernel?\n");
    printf("Q3. What is socket activation in systemd and why does it improve boot time?\n");
    printf("Q4. How does Docker/Kubernetes ensure a container's PID 1 properly reaps zombies?\n");
    printf("    (hint: tini, dumb-init, and prctl(PR_SET_CHILD_SUBREAPER))\n");
    printf("Q5. What is the difference between PID 1 on a bare-metal host (systemd) vs\n");
    printf("    inside a container (e.g., tini), in terms of signal handling?\n");
    printf("Q6. What is a PID namespace and how does it make container PID 1 = host PID N?\n");
    printf("Q7. How does systemd use cgroups v2 to assign each service its own slice,\n");
    printf("    and why does this enable per-service OOM killing and resource limits?\n");
    return 0;
}
