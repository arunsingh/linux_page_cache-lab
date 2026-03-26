#!/usr/bin/env bash
#
# Author: Arun Singh | arunsingh.in@gmail.com
# OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
set -euo pipefail
cd "$(dirname "$0")/.."

LAB_NUM="${1:-}"
if [[ -z "$LAB_NUM" ]]; then
    echo "Usage: ./scripts/run_lab.sh <lab_number>"
    echo ""
    echo "Examples:"
    echo "  ./scripts/run_lab.sh 01     # Threads, Address Spaces"
    echo "  ./scripts/run_lab.sh 15     # Context Switching"
    echo "  ./scripts/run_lab.sh 27     # Demand Paging (original)"
    echo "  ./scripts/run_lab.sh 40     # GPU OS Concepts"
    echo ""
    echo "Available labs:"
    for f in bin/lab_*; do
        [[ -x "$f" ]] && echo "  $(basename $f)"
    done
    [[ -x bin/demand_paging ]] && echo "  demand_paging (lab 27)"
    [[ -x bin/page_cache_models ]] && echo "  page_cache_models"
    exit 0
fi

# Normalize to 2-digit (strip leading zeros first to avoid octal interpretation)
LAB_NUM=$(printf "%02d" "$((10#$LAB_NUM))")

if [[ "$LAB_NUM" == "27" ]] && [[ -x bin/demand_paging ]]; then
    echo "=== Running Lab 27: Demand Paging (original) ==="
    echo "TIP: Open another terminal and run: ./scripts/monitor.sh <PID>"
    echo ""
    exec ./bin/demand_paging
fi

BIN="bin/lab_${LAB_NUM}"
if [[ ! -x "$BIN" ]]; then
    echo "Lab $LAB_NUM not built. Running build_all.sh first..."
    ./scripts/build_all.sh
fi

if [[ -x "$BIN" ]]; then
    exec "$BIN"
else
    echo "ERROR: Lab $LAB_NUM binary not found after build."
    exit 1
fi
