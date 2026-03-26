#!/usr/bin/env bash
# .devcontainer/install.sh
# Installs all tools needed for the OS fundamentals labs.
# Runs once at container creation time (onCreateCommand).
#
# Author: Arun Singh | arunsingh.in@gmail.com
set -euo pipefail

export DEBIAN_FRONTEND=noninteractive

apt-get update -qq
apt-get install -y --no-install-recommends \
    build-essential \
    gcc \
    gdb \
    make \
    strace \
    ltrace \
    linux-perf \
    procps \
    htop \
    sysstat \
    util-linux \
    numactl \
    bc \
    git \
    curl \
    man-db \
    manpages-dev \
    time \
    sudo

# perf symlink — Debian names it perf_<kernel-version>
PERF_BIN=$(ls /usr/bin/perf_* 2>/dev/null | head -1 || true)
if [[ -n "$PERF_BIN" && ! -e /usr/local/bin/perf ]]; then
    ln -s "$PERF_BIN" /usr/local/bin/perf
    echo "Linked $PERF_BIN -> /usr/local/bin/perf"
fi

# Loosen perf permissions for hardware counter labs
echo -1 > /proc/sys/kernel/perf_event_paranoid 2>/dev/null || \
    echo "Note: perf_event_paranoid requires --privileged; most labs work without it."

echo "Dev container install complete."
