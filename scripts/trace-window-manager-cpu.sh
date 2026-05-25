#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${LWM_BUILD_DIR:-$ROOT/build}"
LOG_DIR="${LWM_TRACE_DIR:-$ROOT/.debug-logs}"
LOG_PATH="${LWM_TRACE_LOG:-$LOG_DIR/lwm-cpu.log}"
SAMPLE_TRACE="${LWM_SAMPLE_TRACE:-1}"
SAMPLE_USEC="${LWM_SAMPLE_USEC:-2000}"

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  cat <<EOF
Usage: scripts/trace-window-manager-cpu.sh

Builds and runs lambda-window-manager with CPU tracing enabled.

Environment overrides:
  LWM_BUILD_DIR    Build directory. Default: $BUILD_DIR
  LWM_TRACE_DIR    Directory for logs. Default: $LOG_DIR
  LWM_TRACE_LOG    CPU trace log path. Default: $LOG_PATH
  LWM_SAMPLE_TRACE Enable stack sampling. Default: $SAMPLE_TRACE
  LWM_SAMPLE_USEC  CPU sampler interval in microseconds. Default: $SAMPLE_USEC
EOF
  exit 0
fi

mkdir -p "$LOG_DIR"
rm -f "$LOG_PATH"

cmake --build "$BUILD_DIR" \
  --target lambda-window-manager lambda-shell lambda-files lambda-settings lambda-terminal \
  -j"$(nproc)"

echo "Writing CPU trace to: $LOG_PATH"
echo "Stop the window manager with Ctrl+C after the bad idle/high-CPU case has run for 15-30 seconds."

cd "$ROOT"
LAMBDA_WINDOW_MANAGER_CPU_TRACE=1 \
LAMBDA_WINDOW_MANAGER_CPU_TRACE_LOG="$LOG_PATH" \
LAMBDA_WINDOW_MANAGER_SAMPLE_TRACE="$SAMPLE_TRACE" \
LAMBDA_WINDOW_MANAGER_SAMPLE_USEC="$SAMPLE_USEC" \
"$BUILD_DIR/lambda-window-manager"
