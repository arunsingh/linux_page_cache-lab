#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p bin
cc -O0 -Wall -pthread -o bin/demand_paging labs/demand_paging.c
cc -O2 -Wall -pthread -o bin/page_cache_models labs/page_cache_models.c
chmod +x scripts/monitor.sh
printf "Built:\n  bin/demand_paging\n  bin/page_cache_models\n"
