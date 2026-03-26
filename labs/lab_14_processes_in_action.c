/*
 * lab_14.c
 * Topic: Processes in action
 * Build: gcc -O0 -Wall -pthread -o lab_14 lab_14_*.c
 *
 * Author: Arun Singh | arunsingh.in@gmail.com
 * OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
static void print_section(const char *t){printf("\n========== %s ==========\n",t);}
int main(void){
    printf("=== Lab 14: Processes in Action ===\n");
    print_section("Phase 1: Fork, Exec, Wait");
    printf("  Parent PID: %d, PPID: %d\n",getpid(),getppid());
    for(int i=0;i<3;i++){
        pid_t p=fork();
        if(p==0){
            printf("  Child %d: PID=%d PPID=%d\n",i,getpid(),getppid());
            if(i==2){
                printf("  Child 2: exec(ls)...\n");
                execlp("ls","ls","-la","/proc/self",NULL);
                perror("exec");
            }
            _exit(i);
        }
    }
    for(int i=0;i<3;i++){
        int status;pid_t w=wait(&status);
        if(WIFEXITED(status))printf("  Reaped PID %d, exit=%d\n",w,WEXITSTATUS(status));
    }
    print_section("Phase 2: Process Tree");
    printf("  Run: pstree -p %d\n",getpid());
    printf("  Or:  cat /proc/%d/status | grep -E \"Pid|PPid|Threads\"\n",getpid());
    /* OBSERVE: Between fork() and wait(), the child appears as state 'Z' in ps.
     *          The kernel keeps the task_struct alive so the parent can read exit status.
     *          Only the mm_struct (address space) is freed -- the PCB entry remains. */
    print_section("Phase 3: Zombie and Orphan");
    pid_t z=fork();
    if(z==0)_exit(0);
    printf("  Created child PID %d -- now a zombie until we wait().\n",z);
    printf("  Check: ps aux | grep Z | grep %d\n",z);
    usleep(100000);
    wait(NULL);
    printf("  Reaped zombie.\n");

    /* WHY: /proc/PID/status exposes the kernel's task_struct fields in text form.
     *      VmPeak, VmRSS, VmData track memory usage post-fork and post-exec.
     *      Threads field shows how many threads share this mm_struct.
     *      In Linux 6.x, task_struct is allocated from the kmalloc-2k slab cache. */
    print_section("Phase 4: Reading /proc/self/status Vm Fields");
    {
        char path[64];
        snprintf(path, sizeof(path), "/proc/%d/status", getpid());
        FILE *fp = fopen(path, "r");
        if (fp) {
            char line[256];
            printf("  Key fields from /proc/self/status:\n");
            while (fgets(line, sizeof(line), fp)) {
                if (strncmp(line,"Pid:",4)==0 || strncmp(line,"PPid:",5)==0 ||
                    strncmp(line,"VmPeak:",7)==0 || strncmp(line,"VmRSS:",6)==0 ||
                    strncmp(line,"VmData:",7)==0 || strncmp(line,"VmStk:",6)==0 ||
                    strncmp(line,"Threads:",8)==0 || strncmp(line,"NSpid:",6)==0)
                    printf("    %s", line);
            }
            fclose(fp);
        }
    }
    /* OBSERVE: NSpid shows both the host PID and container-namespace PID (if in one).
     *          This is how Docker maps host PIDs to container PIDs -- same process,
     *          different view due to PID namespace. */
    printf("  NSpid: shows host PID and PID-namespace-local PID (useful in containers).\n");

    /* === EXERCISE ===
     * Try these hands-on tasks:
     * 1. Fork a child, have it sleep 10 seconds, and in the parent read
     *    /proc/<child_pid>/status.  Record VmRSS and VmPeak.  Then have the child
     *    exec a different program (e.g., /bin/sleep 1).  Re-read status and observe
     *    that VmRSS resets because execve() replaces the entire address space.
     * 2. Create a zombie: fork(), child calls _exit(0) immediately, parent sleeps 5s
     *    without calling wait().  In another terminal run: ps -eo pid,stat,comm | grep Z
     *    Note the child's entry.  Then call wait() and verify it disappears.
     * 3. Modern (pid namespaces in containers): run this inside a Docker container
     *    and compare /proc/self/status NSpid field.  The first number is host PID,
     *    second is container-local PID.  This is how 'docker top' maps container PIDs
     *    back to host PIDs without entering the namespace.
     *
     * OBSERVE: After execve(), open file descriptors (stdout, stderr) survive unless
     *          O_CLOEXEC was set.  The PID, PPID, and signal dispositions are preserved.
     * WHY:     fork()+execve() separation lets the shell set up fd redirections (dup2)
     *          between the two calls, before the new program starts.  A combined
     *          posix_spawn() requires a separate attribute object for every fd operation.
     */
    printf("\n========== Hands-On Exercise ==========\n");
    printf("1. Fork a child, read /proc/<child>/status VmRSS before and after execve;\n");
    printf("   observe that execve() resets the entire address space.\n");
    printf("2. Create a zombie (child exits, parent sleeps 5s), observe 'Z' state in ps.\n");
    printf("3. Modern: run in Docker and inspect /proc/self/status NSpid to see\n");
    printf("   host vs container PID namespace mapping.\n");

    printf("\n========== Quiz ==========\n");
    printf("Q1. What is a zombie process and how is it created?\n");
    printf("Q2. What happens to orphaned processes?\n");
    printf("Q3. What does execve() replace in the process? What does it preserve?\n");
    printf("Q4. Why is fork()+execve() the standard pattern instead of a single spawn()?\n");
    printf("Q5. What fields in /proc/PID/status track memory usage, and which resets after execve()?\n");
    printf("Q6. What is a PID namespace and how does it enable container PID isolation\n");
    printf("    (hint: clone(CLONE_NEWPID) and the NSpid field in /proc/status)?\n");
    printf("Q7. How does systemd (PID 1) avoid zombie accumulation when it adopts\n");
    printf("    orphaned processes (hint: prctl(PR_SET_CHILD_SUBREAPER))?\n");
    return 0;
}
