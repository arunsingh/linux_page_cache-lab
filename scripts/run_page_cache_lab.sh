#!/usr/bin/env bash
#
# Author: Arun Singh | arunsingh.in@gmail.com
# OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
set -euo pipefail
cd "$(dirname "$0")/.."
FILE="${1:-../testdata.bin}"
if [[ ! -x bin/page_cache_models ]]; then
  ./scripts/build.sh
fi
if [[ ! -f "$FILE" ]]; then
  echo "Test file not found: $FILE"
  echo "Run: ./scripts/setup_testdata.sh 128"
  exit 1
fi
printf "\n=== single ===\n"
./bin/page_cache_models "$FILE" single 1 1 2
printf "\n=== threads ===\n"
./bin/page_cache_models "$FILE" threads 1 4 2
printf "\n=== processes ===\n"
./bin/page_cache_models "$FILE" processes 4 1 2
printf "\n=== hybrid ===\n"
./bin/page_cache_models "$FILE" hybrid 2 2 2
