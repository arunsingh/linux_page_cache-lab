/*
 * lab_12_kernel_boot.c
 * Topic: Loading the Kernel, Initializing the Page Table
 * Parses dmesg for boot timeline, shows kernel command line, early memory setup.
 * Build: gcc -O0 -Wall -o lab_12 lab_12_kernel_boot.c
 *
 * Author: Arun Singh | arunsingh.in@gmail.com
 * OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
*/
#include <stdio.h>
#include <string.h>
#include <unistd.h>
static void print_section(const char *t){printf("\n========== %s ==========\n",t);}

static void demo_cmdline(void){
    print_section("Phase 1: Kernel Command Line");
    FILE *fp=fopen("/proc/cmdline","r");
    if(fp){char l[1024];if(fgets(l,sizeof(l),fp))printf("  %s\n",l);fclose(fp);}
    printf("  Key parameters: root= (root fs), console= (serial), hugepagesz=, isolcpus=\n");
    printf("  GPU DC kernels often have: iommu=pt default_hugepagesz=1G isolcpus=1-N\n");
}

static void demo_boot_memory(void){
    print_section("Phase 2: Early Memory Setup (dmesg)");
    const char *patterns[]={"Memory:","BIOS-e820:","page tables","kernel","Zone",NULL};
    FILE *fp=popen("dmesg 2>/dev/null","r");
    if(!fp){printf("  (need root)\n");return;}
    char line[512];int shown=0;
    while(fgets(line,sizeof(line),fp)&&shown<25){
        for(int i=0;patterns[i];i++){
            if(strstr(line,patterns[i])){printf("  %s",line);shown++;break;}
        }
    }
    pclose(fp);
    printf("\n  OBSERVE: BIOS-e820 provides the physical memory map to the kernel.\n");
    printf("  The kernel builds its initial page tables to map itself and the direct-map region.\n");
}

static void demo_kernel_version(void){
    print_section("Phase 3: Kernel Version & Config");
    FILE *fp=fopen("/proc/version","r");
    if(fp){char l[512];if(fgets(l,sizeof(l),fp))printf("  %s",l);fclose(fp);}
    printf("\n  Current kernel config highlights:\n");
    fp=popen("zcat /proc/config.gz 2>/dev/null | grep -E 'HUGETLB|TRANSPARENT_HUGE|NUMA|PREEMPT|HZ=' | head -10","r");
    if(fp){char l[256];while(fgets(l,sizeof(l),fp))printf("  %s",l);pclose(fp);}
    else printf("  (/proc/config.gz not available — check /boot/config-*)\n");
}

/* WHY: Measured boot uses a TPM 2.0 chip to record hashes of each boot component
 *      (firmware, bootloader, kernel, initrd) into PCR (Platform Configuration Register)
 *      banks.  This creates a tamper-evident boot chain.  Cloud providers (Azure, GCP,
 *      AWS) use measured boot to verify that a VM's kernel has not been tampered with.
 *      Linux's IMA (Integrity Measurement Architecture) extends this to userspace files.
 *      The Linux kernel exposes TPM2 via /dev/tpm0 and /dev/tpmrm0 (kernel resource manager).
 *
 * WHY: UEFI Secure Boot verifies each component's signature against a key database (db).
 *      The bootloader (GRUB2 with SHIM) is signed by Microsoft or distro CA.  The kernel
 *      must also be signed.  Unsigned kernel modules are rejected when Secure Boot is active.
 *      This is why 'insmod' of out-of-tree modules fails on Secure Boot systems unless
 *      the module is signed with a key enrolled in the MOK (Machine Owner Key) database.
 */

static void demo_kexec_tpm(void){
    print_section("Phase 4: Measured Boot, kexec, and TPM2");

    /* Check kexec status */
    FILE *fp = fopen("/sys/kernel/kexec_loaded", "r");
    if (fp) {
        char val[8] = {0};
        if (fgets(val, sizeof(val), fp)) {
            val[strcspn(val, "\n")] = '\0';
            printf("  kexec_loaded: %s\n", val);
            printf("  (1=kernel is loaded and ready for kexec reboot without BIOS/UEFI)\n");
        }
        fclose(fp);
    } else {
        printf("  /sys/kernel/kexec_loaded: not available\n");
    }

    fp = fopen("/sys/kernel/kexec_crash_loaded", "r");
    if (fp) {
        char val[8] = {0};
        if (fgets(val, sizeof(val), fp)) {
            val[strcspn(val, "\n")] = '\0';
            printf("  kexec_crash_loaded: %s  (kdump kernel pre-loaded for crash capture)\n", val);
        }
        fclose(fp);
    }

    /* Check TPM2 */
    printf("\n  TPM2 device check:\n");
    if (access("/dev/tpm0", F_OK) == 0) {
        printf("  /dev/tpm0: present (TPM2 hardware available)\n");
    } else {
        printf("  /dev/tpm0: not present (no TPM, or module not loaded)\n");
    }
    if (access("/dev/tpmrm0", F_OK) == 0) {
        printf("  /dev/tpmrm0: present (kernel TPM resource manager)\n");
    }

    /* Secure Boot status */
    printf("\n  Secure Boot status:\n");
    fp = fopen("/sys/firmware/efi/efivars/SecureBoot-8be4df61-93ca-11d2-aa0d-00e098032b8c", "r");
    if (fp) {
        unsigned char buf[5] = {0};
        size_t n = fread(buf, 1, sizeof(buf), fp);
        fclose(fp);
        if (n == 5)
            printf("  Secure Boot: %s\n", buf[4] ? "ENABLED" : "DISABLED");
    } else {
        printf("  Secure Boot EFI variable: not readable (not UEFI or no access)\n");
    }

    /* OBSERVE: kexec allows loading a new kernel image in memory and switching to it
     *          without going through BIOS/UEFI POST.  Used for fast reboots and kdump.
     *          On cloud VMs, kexec is used for live kernel patching (livepatch). */
    printf("\n  WHY kexec: switches to a new kernel in ~100ms vs ~30s for full reboot.\n");
    printf("  Used by kdump (crash kernel), kernel live patching, and fast provisioning.\n");
}

int main(void){
    /* EXPECTED OUTPUT (Linux x86_64):
     *   Phase 1: /proc/cmdline content (e.g., BOOT_IMAGE=/vmlinuz root=UUID=... quiet)
     *   Phase 2: dmesg lines matching Memory, e820, page tables, Zone keywords
     *   Phase 3: /proc/version (Linux kernel version string); config.gz highlights
     *   Phase 4: kexec_loaded=0 (normally); kexec_crash_loaded=1 (if kdump configured);
     *            TPM0 present/absent; Secure Boot enabled/disabled
     */
    printf("=== Lab 12: Loading the Kernel, Initializing Page Tables ===\n");
    demo_cmdline();
    demo_boot_memory();
    demo_kernel_version();
    demo_kexec_tpm();

    /* === EXERCISE ===
     * Try these hands-on tasks:
     * 1. Read /proc/cmdline and parse each parameter.  Find the 'root=' parameter
     *    and determine if it specifies a UUID, device path, or label.  Then cross-
     *    reference with 'lsblk -f' to find which block device it maps to.
     * 2. Check /sys/kernel/kexec_loaded.  If you have root access, use kexec-tools:
     *      kexec -l /boot/vmlinuz --initrd=/boot/initrd.img --reuse-cmdline
     *    to stage the current kernel for a soft reboot.  Verify kexec_loaded becomes 1.
     * 3. Modern (measured boot + TPM2 in cloud): on a GCP or Azure VM with vTPM:
     *      tpm2_pcrread  (from tpm2-tools package)
     *    Read PCR[0]-PCR[7] to see what was measured during boot.  PCR[8]-PCR[9] contain
     *    GRUB measurements.  If IMA is enabled, PCR[10] contains kernel file measurements.
     *    This chain proves to a remote verifier that the exact software stack is running.
     *
     * OBSERVE: The kernel command line (from GRUB) controls key behaviors:
     *          - 'nopti': disable KPTI (Meltdown mitigation) -- dangerous but faster
     *          - 'iommu=pt': passthrough mode for GPU servers (less IOMMU overhead)
     *          - 'transparent_hugepage=always': enable THP at boot
     *          - 'isolcpus=N-M': reserve CPUs for real-time tasks
     * WHY:     GPU datacenter kernels commonly add 'iommu=pt' because the IOMMU in
     *          passthrough mode does no address translation (GPU physical = host physical),
     *          eliminating IOTLB miss overhead for high-bandwidth NVLink/PCIe transfers.
     */
    printf("\n========== Hands-On Exercise ==========\n");
    printf("1. Parse /proc/cmdline; find root= UUID and map to block device via lsblk.\n");
    printf("2. Use kexec-tools to stage current kernel; verify kexec_loaded becomes 1.\n");
    printf("3. Modern (TPM2 measured boot): use tpm2_pcrread to inspect PCR[0-10];\n");
    printf("   explain how PCR values prove the software stack to a remote verifier.\n");

    printf("\n========== Quiz ==========\n");
    printf("Q1. What is the e820 memory map and who provides it?\n");
    printf("Q2. What is the kernel direct-map region and why does Linux map all physical RAM there?\n");
    printf("Q3. What does 'iommu=pt' on the kernel command line do for GPU servers?\n");
    printf("Q4. Why does the kernel need early page tables before the memory allocator is ready?\n");
    printf("Q5. What is kexec and how does it differ from a normal reboot?\n");
    printf("    Why is it used for kdump (kernel crash capture)?\n");
    printf("Q6. Explain UEFI Secure Boot: what is signed, who holds the signing keys,\n");
    printf("    and why does it block unsigned out-of-tree kernel modules?\n");
    printf("Q7. How does TPM2 measured boot work?  What is a PCR, and how do cloud\n");
    printf("    providers use PCR values to attest that a VM is running trusted software?\n");
    return 0;
}
