#!/usr/bin/env bash
#
# Author: Arun Singh | arunsingh.in@gmail.com
# OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p bin

echo "Building all OS labs..."

# Map lab numbers to source files
build_lab() {
    local num=$1
    local src=$(ls labs/lab_${num}_*.c 2>/dev/null | head -1)
    if [[ -z "$src" ]]; then
        echo "  SKIP lab_${num}: source not found"
        return
    fi
    local out="bin/lab_${num}"
    if cc -O0 -Wall -pthread -o "$out" "$src" -lm 2>/dev/null; then
        echo "  OK   $out <- $src"
    else
        echo "  FAIL $out <- $src"
    fi
}

# Build original labs
if [[ -f labs/demand_paging.c ]]; then
    cc -O0 -Wall -pthread -o bin/demand_paging labs/demand_paging.c 2>/dev/null && \
        echo "  OK   bin/demand_paging" || echo "  FAIL bin/demand_paging"
fi
if [[ -f labs/page_cache_models.c ]]; then
    cc -O2 -Wall -pthread -o bin/page_cache_models labs/page_cache_models.c 2>/dev/null && \
        echo "  OK   bin/page_cache_models" || echo "  FAIL bin/page_cache_models"
fi

# Build all numbered labs
for num in $(seq -w 1 40); do
    # Skip 27 (use original demand_paging)
    [[ "$num" == "27" ]] && continue
    build_lab "$num"
done

chmod +x scripts/*.sh 2>/dev/null || true

echo ""
echo "Build complete. Run: ./scripts/run_lab.sh <number>"
echo "Example: ./scripts/run_lab.sh 01"
