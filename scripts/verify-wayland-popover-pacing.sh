#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${LWM_WAYLAND_BUILD_DIR:-$ROOT/build}"
LOG_ROOT="${LWM_WAYLAND_POPOVER_PACING_LOG_DIR:-$ROOT/.debug-logs/wayland-popover-pacing}"
RUN_ID="$(date +%Y%m%d-%H%M%S)"
LOG_DIR="$LOG_ROOT/$RUN_ID"
FORCE_FIFO="${LAMBDA_WAYLAND_POPOVER_PACING_FORCE_FIFO:-1}"
TIMEOUT_SECONDS="${LAMBDA_WAYLAND_POPOVER_PACING_TIMEOUT_SECONDS:-10}"

usage() {
  cat <<EOF
Usage: scripts/verify-wayland-popover-pacing.sh

Runs a focused Linux Wayland popover pacing check under headless Weston GL:
  - builds popover-demo
  - starts Weston headless with a fake seat
  - runs popover-demo in timer-driven autotest mode
  - asserts initial popover paint is immediate
  - asserts committed popover updates render only from Wayland frame callbacks

Environment:
  LWM_WAYLAND_BUILD_DIR                         Wayland build dir. Default: $BUILD_DIR
  LWM_WAYLAND_POPOVER_PACING_LOG_DIR            Log root. Default: $LOG_ROOT
  LAMBDA_WAYLAND_POPOVER_PACING_FORCE_FIFO      Force FIFO present mode. Default: $FORCE_FIFO
  LAMBDA_WAYLAND_POPOVER_PACING_TIMEOUT_SECONDS App timeout. Default: $TIMEOUT_SECONDS
EOF
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  usage
  exit 0
fi

command -v weston >/dev/null 2>&1 || {
  echo "weston is required for Wayland popover pacing verification." >&2
  exit 2
}

mkdir -p "$LOG_DIR"
export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"

cmake --build "$BUILD_DIR" --target popover-demo -j"$(nproc)"

SOCKET="lambda-weston-popover-$$"
WESTON_PID=""
cleanup() {
  set +e
  if [[ -n "$WESTON_PID" ]]; then
    kill -TERM "$WESTON_PID" 2>/dev/null || true
    sleep 1
    kill -KILL "$WESTON_PID" 2>/dev/null || true
    wait "$WESTON_PID" 2>/dev/null || true
  fi
  rm -f "$XDG_RUNTIME_DIR/$SOCKET" "$XDG_RUNTIME_DIR/$SOCKET.lock"
  set -e
}
trap cleanup EXIT

weston --backend=headless \
  --renderer=gl \
  --width=1280 \
  --height=720 \
  --idle-time=0 \
  --fake-seat \
  --socket="$SOCKET" \
  --no-config \
  --log="$LOG_DIR/weston.log" >"$LOG_DIR/weston-stdout.log" 2>&1 &
WESTON_PID=$!

for _ in $(seq 1 100); do
  if [[ -S "$XDG_RUNTIME_DIR/$SOCKET" ]]; then
    break
  fi
  if ! kill -0 "$WESTON_PID" 2>/dev/null; then
    echo "Weston exited before creating $SOCKET" >&2
    tail -n 120 "$LOG_DIR/weston.log" >&2 || true
    exit 20
  fi
  sleep 0.1
done

if [[ ! -S "$XDG_RUNTIME_DIR/$SOCKET" ]]; then
  echo "Weston socket $SOCKET was not created." >&2
  tail -n 120 "$LOG_DIR/weston.log" >&2 || true
  exit 20
fi

set +e
WAYLAND_DISPLAY="$SOCKET" \
  LAMBDA_WAYLAND_POPOVER_TRACE=1 \
  LAMBDA_POPOVER_DEMO_AUTOTEST=1 \
  LAMBDA_DEBUG_PERF=2 \
  LAMBDA_VULKAN_FORCE_FIFO_PRESENT_MODE="$FORCE_FIFO" \
  timeout --signal=TERM --kill-after=2s "${TIMEOUT_SECONDS}s" \
  "$BUILD_DIR/demos/popover-demo" >"$LOG_DIR/popover-demo.log" 2>&1
app_status=$?
set -e

if [[ "$app_status" -ne 0 ]]; then
  echo "SUMMARY wayland-popover-pacing app_status=$app_status log_dir=$LOG_DIR"
  tail -n 160 "$LOG_DIR/popover-demo.log" >&2 || true
  exit 1
fi

fatal_count=$( (rg -n -i 'fatal|segmentation|assert|AddressSanitizer|VK_ERROR|present failed|surface lost|render error' \
  "$LOG_DIR/popover-demo.log" "$LOG_DIR/weston.log" || true) | wc -l )
if [[ "$fatal_count" -ne 0 ]]; then
  echo "SUMMARY wayland-popover-pacing fatal_matches=$fatal_count log_dir=$LOG_DIR"
  rg -n -i 'fatal|segmentation|assert|AddressSanitizer|VK_ERROR|present failed|surface lost|render error' \
    "$LOG_DIR/popover-demo.log" "$LOG_DIR/weston.log" >&2 || true
  exit 1
fi

awk -v label="wayland-popover-pacing" '
  /wayland-popover-detail:/ && /event=render/ && /reason=initial-configure/ && /committed_before=0/ {
    initial_render += 1
  }
  /wayland-popover:/ && /event=redraw-request/ && /committed=1/ {
    committed_redraw += 1
  }
  /wayland-popover:/ && /event=frame-request/ {
    frame_request += 1
  }
  /wayland-popover:/ && /event=frame-done/ {
    frame_done += 1
  }
  /wayland-popover-detail:/ && /event=render/ && /reason=frame-callback/ && /committed_before=1/ {
    paced_render += 1
  }
  /wayland-popover-detail:/ && /event=render/ && /reason=redraw/ && /committed_before=1/ {
    immediate_committed_render += 1
  }
  /\[popover-demo-autotest\] show/ {
    autotest_show += 1
  }
  /\[popover-demo-autotest\] update/ {
    autotest_update += 1
  }
  /\[popover-demo-autotest\] complete/ {
    autotest_complete += 1
  }
  END {
    printf "SUMMARY %s initial_render=%d committed_redraw=%d frame_request=%d frame_done=%d paced_render=%d immediate_committed_render=%d autotest_show=%d autotest_update=%d autotest_complete=%d\n",
      label, initial_render, committed_redraw, frame_request, frame_done, paced_render,
      immediate_committed_render, autotest_show, autotest_update, autotest_complete
    if (initial_render < 1 || committed_redraw < 2 || frame_request < 2 || frame_done < 1 ||
        paced_render < 1 || immediate_committed_render != 0 ||
        autotest_show != 1 || autotest_update < 4 || autotest_complete != 1) {
      exit 1
    }
  }
' "$LOG_DIR/popover-demo.log"

echo "CASE wayland-popover-pacing renderer=gl log_dir=$LOG_DIR"
echo "Wayland popover pacing verification completed."
