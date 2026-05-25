#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${LWM_BUILD_DIR:-$ROOT/build}"
TRACE_DIR="${LWM_SNAP_TRACE_DIR:-$ROOT/.debug-logs/snap-trace}"
TRACE_FRAMES="${LWM_SNAP_TRACE_FRAMES:-600}"
TRACE_TAIL_FRAMES="${LWM_SNAP_TRACE_TAIL_FRAMES:-12}"
CAPTURE_DIR="${LWM_SNAP_CAPTURE_DIR:-$ROOT/.debug-logs/snap-capture}"
CAPTURE_FRAMES="${LWM_SNAP_CAPTURE_FRAMES:-120}"
TAIL_FRAMES="${LWM_SNAP_CAPTURE_TAIL_FRAMES:-12}"
MODE="trace"

if [[ "${1:-}" == "--capture" ]]; then
  MODE="capture"
  shift
elif [[ "${1:-}" == "--both" ]]; then
  MODE="both"
  shift
fi

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  cat <<EOF
Usage: scripts/trace-snap-flicker.sh [--capture|--both]

Builds and runs lambda-window-manager with low-overhead snap tracing enabled.
Trigger the snap/edge animation once, then stop the window manager.

Default mode records compositor timing/state to CSV without framebuffer readback.
Use --capture only when you explicitly want PNG frames; it will slow animations.

Environment overrides:
  LWM_BUILD_DIR                  Build directory. Default: $BUILD_DIR
  LWM_SNAP_TRACE_DIR             Trace root. Default: $TRACE_DIR
  LWM_SNAP_TRACE_FRAMES          Max trace frames. Default: $TRACE_FRAMES
  LWM_SNAP_TRACE_TAIL_FRAMES     Frames after animation ends. Default: $TRACE_TAIL_FRAMES
  LWM_SNAP_CAPTURE_DIR           Capture root. Default: $CAPTURE_DIR
  LWM_SNAP_CAPTURE_FRAMES        Max PNG frames in --capture/--both. Default: $CAPTURE_FRAMES
  LWM_SNAP_CAPTURE_TAIL_FRAMES   PNG frames after animation ends. Default: $TAIL_FRAMES
EOF
  exit 0
fi

mkdir -p "$TRACE_DIR" "$CAPTURE_DIR"

cmake --build "$BUILD_DIR" \
  --target lambda-window-manager lambda-shell lambda-files lambda-settings lambda-terminal \
  -j"$(nproc)"

echo "Snap flicker trace:"
echo "  mode:       $MODE"
echo "  trace root: $TRACE_DIR"
echo "  max frames: $TRACE_FRAMES"
if [[ "$MODE" == "capture" || "$MODE" == "both" ]]; then
  echo "  capture root: $CAPTURE_DIR"
  echo "  PNG frames:   $CAPTURE_FRAMES"
fi
echo
echo "Trigger the bad snap animation once. The compositor will print the exact trace"
echo "directory. PNG capture is disabled in default mode so animation timing stays real."
echo

cd "$ROOT"

ENV_VARS=(
  "LWM_SNAP_TRACE=1"
  "LWM_SNAP_TRACE_DIR=$TRACE_DIR"
  "LWM_SNAP_TRACE_FRAMES=$TRACE_FRAMES"
  "LWM_SNAP_TRACE_TAIL_FRAMES=$TRACE_TAIL_FRAMES"
)

if [[ "$MODE" == "capture" || "$MODE" == "both" ]]; then
  ENV_VARS+=(
    "LWM_SNAP_CAPTURE_DIR=$CAPTURE_DIR"
    "LWM_SNAP_CAPTURE_FRAMES=$CAPTURE_FRAMES"
    "LWM_SNAP_CAPTURE_TAIL_FRAMES=$TAIL_FRAMES"
  )
fi

env "${ENV_VARS[@]}" "$BUILD_DIR/lambda-window-manager"
