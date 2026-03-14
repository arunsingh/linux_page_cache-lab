#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
if [[ ! -x bin/demand_paging ]]; then
  ./scripts/build.sh
fi
printf "Open another terminal and run: ./scripts/monitor.sh <PID shown by the program>\n\n"
exec ./bin/demand_paging
