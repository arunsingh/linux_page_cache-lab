# Dockerfile — Linux OS Fundamentals Labs
# Author: Arun Singh | arunsingh.in@gmail.com
# OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
#
# Usage:
#   docker build -t os-labs .
#   docker run -it --rm --cap-add=SYS_PTRACE --security-opt seccomp=unconfined \
#       -v "$(pwd)":/os-labs -w /os-labs os-labs bash

FROM debian:bullseye-slim

LABEL org.opencontainers.image.title="Linux OS Fundamentals Labs"
LABEL org.opencontainers.image.description="40-lab series: demand paging, page cache, TLB, scheduler, containers"
LABEL org.opencontainers.image.authors="Arun Singh <arunsingh.in@gmail.com>"
LABEL org.opencontainers.image.source="https://github.com/arunsingh/linux_page_cache-lab"

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update -qq && \
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
        bash \
    && \
    # perf symlink
    PERF_BIN=$(ls /usr/bin/perf_* 2>/dev/null | head -1 || true) && \
    if [ -n "$PERF_BIN" ]; then ln -s "$PERF_BIN" /usr/local/bin/perf; fi && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /os-labs

# Copy source on build (can also be bind-mounted at runtime — see docker-compose.yml)
COPY . .

RUN bash scripts/build_all.sh

# Default: interactive bash with a helpful banner
CMD ["bash", "--login"]
