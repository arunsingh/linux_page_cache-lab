#!/usr/bin/env bash
set -euo pipefail
SIZE_MIB="${1:-128}"
cd "$(dirname "$0")/.."
dd if=/dev/urandom of=testdata.bin bs=1M count="$SIZE_MIB" status=progress
