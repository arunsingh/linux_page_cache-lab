# -*- mode: ruby -*-
# vi: set ft=ruby :
#
# Vagrant sandbox for Linux OS Fundamentals Labs
# Author: Arun Singh | arunsingh.in@gmail.com
# OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
#
# Inspired by:
#   https://iitd-os.github.io/os-nptel/
#   https://www.cse.iitd.ac.in/~sbansal/os/
#
# Quick start:
#   vagrant up          # first boot: installs tools + builds all 40 labs
#   vagrant ssh         # open shell inside the VM
#   ./scripts/run_lab.sh 01
#
# Tips:
#   vagrant halt        # stop VM (saves disk snapshot)
#   vagrant suspend     # pause VM (fastest resume)
#   vagrant rsync       # push host changes -> guest
#   vagrant rsync-auto  # watch + auto-push on save
#   vagrant destroy -f  # nuclear reset

Vagrant.configure("2") do |config|

  # ---------------------------------------------------------------------------
  # Box
  # ---------------------------------------------------------------------------
  # Debian 11 Bullseye: stable LTS, matches most university Linux servers.
  # Kernel 5.10 LTS ships by default; upgrade to 6.x via provision if needed.
  config.vm.box         = "debian/bullseye64"
  config.vm.box_version = "11.20241217.1"
  config.vm.hostname    = "os-labs"

  # ---------------------------------------------------------------------------
  # SSH
  # ---------------------------------------------------------------------------
  config.ssh.insert_key = true

  # ---------------------------------------------------------------------------
  # Network
  # ---------------------------------------------------------------------------
  # Nothing exposed publicly by default. Uncomment for web-based tools.
  # config.vm.network "forwarded_port", guest: 8080, host: 8080, host_ip: "127.0.0.1"

  # ---------------------------------------------------------------------------
  # Synced folder  (rsync — no Guest Additions version dependency)
  # ---------------------------------------------------------------------------
  config.vm.synced_folder ".", "/home/vagrant/os-labs", type: "rsync",
    rsync__exclude: [".vagrant/", ".git/", "bin/"],
    rsync__args:    ["--archive", "--delete", "--compress"]

  # ---------------------------------------------------------------------------
  # Provider: VirtualBox
  # ---------------------------------------------------------------------------
  config.vm.provider "virtualbox" do |vb|
    vb.name   = "os-labs"
    vb.memory = "4096"   # 4 GB: room for page-cache fill experiments
    vb.cpus   = 2        # 2 CPUs: context-switch + multi-process labs

    # Paravirt provider = KVM: better VM clock & scheduler accuracy
    vb.customize ["modifyvm", :id, "--paravirtprovider", "kvm"]

    # Enable PAE/NX for x86 page table labs
    vb.customize ["modifyvm", :id, "--pae", "on"]

    # Disable USB to reduce footprint
    vb.customize ["modifyvm", :id, "--usb", "off"]
  end

  # ---------------------------------------------------------------------------
  # Provision: one-shot bootstrap (runs only on first `vagrant up`)
  # ---------------------------------------------------------------------------
  config.vm.provision "shell", name: "bootstrap", privileged: true, inline: <<~'SHELL'
    set -euo pipefail
    echo "========================================"
    echo " OS Labs bootstrap — $(date)"
    echo "========================================"

    # ---- Package install ----
    export DEBIAN_FRONTEND=noninteractive
    apt-get update -qq
    apt-get install -y --no-install-recommends \
        build-essential gcc gdb make \
        strace ltrace \
        linux-perf procps \
        htop iotop sysstat \
        util-linux \
        numactl \
        bc jq \
        git curl wget \
        man-db manpages-dev \
        bash-completion \
        time \
        2>&1 | grep -E "^(Inst|Err|dpkg)" || true

    # ---- Perf symlink (Debian packages it as linux-perf-<version>) ----
    PERF_BIN=$(ls /usr/bin/perf_* 2>/dev/null | head -1 || true)
    if [[ -n "$PERF_BIN" && ! -e /usr/local/bin/perf ]]; then
        ln -s "$PERF_BIN" /usr/local/bin/perf
    fi

    # ---- Allow perf for all users (needed for hardware counter labs) ----
    echo -1 > /proc/sys/kernel/perf_event_paranoid || true
    echo "kernel.perf_event_paranoid = -1" >> /etc/sysctl.d/99-labs.conf

    # ---- Increase vm.max_map_count for mmap-heavy labs ----
    echo "vm.max_map_count = 262144" >> /etc/sysctl.d/99-labs.conf
    sysctl -p /etc/sysctl.d/99-labs.conf 2>/dev/null || true

    echo "Bootstrap complete."
  SHELL

  # ---- Build labs as vagrant user (not root, so bins are owned correctly) ----
  config.vm.provision "shell", name: "build-labs", privileged: false, inline: <<~'SHELL'
    set -euo pipefail
    cd /home/vagrant/os-labs

    echo ""
    echo "=== Building all 40 labs ==="
    bash scripts/build_all.sh

    echo ""
    echo "=== Generating 128 MB test dataset ==="
    if [[ ! -f testdata.bin ]]; then
        dd if=/dev/urandom of=testdata.bin bs=1M count=128 status=progress 2>&1
    else
        echo "testdata.bin already exists, skipping."
    fi

    echo ""
    echo "====================================================="
    echo " Setup complete!"
    echo "   cd /home/vagrant/os-labs"
    echo "   ./scripts/run_lab.sh 01   # threads & address spaces"
    echo "   ./scripts/run_lab.sh 27   # demand paging"
    echo "   ./scripts/run_lab.sh 40   # GPU / OS concepts"
    echo "====================================================="
  SHELL

  # ---- MOTD shown on every ssh login ----
  config.vm.provision "shell", name: "motd", privileged: true, inline: <<~'SHELL'
    cat > /etc/motd << 'MOTD'

  ╔═══════════════════════════════════════════════════════╗
  ║        Linux OS Fundamentals Labs — Arun Singh       ║
  ║  Inspired by Prof. Sorav Bansal, NPTEL / IIT Delhi   ║
  ╠═══════════════════════════════════════════════════════╣
  ║  cd os-labs                                           ║
  ║  ./scripts/run_lab.sh <01-40>                         ║
  ║  ./scripts/build_all.sh   (rebuild after edits)      ║
  ╚═══════════════════════════════════════════════════════╝

MOTD
  SHELL

end
