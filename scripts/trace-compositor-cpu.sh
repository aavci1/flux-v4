#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${LAMBDA_WINDOW_MANAGER_BIN:-$ROOT/build/lambda-window-manager}"
TRACE_LOG="${LAMBDA_WINDOW_MANAGER_CPU_TRACE_LOG:-/tmp/lambda-window-manager-cpu.log}"
STDERR_LOG="${LAMBDA_WINDOW_MANAGER_STDERR_LOG:-/tmp/lambda-window-manager-compositor.log}"

if [[ ! -x "$BIN" ]]; then
  echo "Compositor binary not found: $BIN" >&2
  echo "Build it first with: cmake --build build --target lambda-window-manager" >&2
  exit 1
fi

: >"$TRACE_LOG"
: >"$STDERR_LOG"

echo "CPU trace log: $TRACE_LOG"
echo "Compositor log: $STDERR_LOG"
echo "Run workload for 10-20 seconds, then inspect the last cpu-trace lines."

export LAMBDA_WINDOW_MANAGER_CPU_TRACE=1
export LAMBDA_WINDOW_MANAGER_SAMPLE_TRACE="${LAMBDA_WINDOW_MANAGER_SAMPLE_TRACE:-1}"
export LAMBDA_WINDOW_MANAGER_SAMPLE_USEC="${LAMBDA_WINDOW_MANAGER_SAMPLE_USEC:-1000}"
export LAMBDA_WINDOW_MANAGER_CPU_TRACE_LOG="$TRACE_LOG"

"$BIN" "$@" 2>&1 | tee "$STDERR_LOG"
