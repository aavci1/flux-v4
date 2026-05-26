#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${LWM_BUILD_DIR:-$ROOT/build}"
LOG_ROOT="${LWM_DEMO_LOG_DIR:-$ROOT/.debug-logs/compositor-demos}"
RUN_ID="$(date +%Y%m%d-%H%M%S)"
LOG_DIR="$LOG_ROOT/$RUN_ID"
SECONDS_PER_DEMO="${LWM_DEMO_SECONDS:-8}"
BUILD=1
INCLUDE_POPUP_GRAB=0
PASSIVE_ONLY=0
SELECTED_DEMO=""

usage() {
  cat <<EOF
Usage: scripts/run-compositor-demos.sh [options]

Runs the in-tree compositor protocol demos against an already-running
lambda-window-manager session. The script auto-detects WAYLAND_DISPLAY from
\$XDG_RUNTIME_DIR/lambda-window-manager-display when it is not already set.

Options:
  --no-build           Do not build demo targets first.
  --include-popup-grab Include popup-grab demo. Requires [input] popup_grabs = true.
  --passive-only       Run only demos that do not need pointer/keyboard interaction.
  --demo NAME          Run one demo by short name, e.g. shm, popup, dnd.
  --seconds N          Seconds to leave each demo open. Default: $SECONDS_PER_DEMO.
  --list               List known demos and exit.
  -h, --help           Show this help.

Environment:
  LWM_BUILD_DIR        Build directory. Default: $BUILD_DIR
  LWM_DEMO_LOG_DIR     Demo log root. Default: $LOG_ROOT
  LWM_DEMO_SECONDS     Seconds per demo. Default: $SECONDS_PER_DEMO
EOF
}

demo_table() {
  cat <<'EOF'
shm|lambda-window-manager-shm-demo|passive|committed|Decorated SHM window appears with rendered content.
dmabuf|lambda-window-manager-dmabuf-demo|passive|committed|Decorated DMABUF window appears; compositor does not hang.
viewport|lambda-window-manager-viewport-demo|passive|committed|Viewported pattern appears and stays inside surface bounds.
fractional-scale|lambda-window-manager-fractional-scale-demo|passive|preferred scale|Preferred scale is printed; content remains sharp.
cursor-shape|lambda-window-manager-cursor-shape-demo|interactive|move pointer|Move over the window; cursor changes to the requested shape.
layer-shell|lambda-window-manager-layer-shell-demo|passive|committed top layer|Top layer-shell bar appears with non-black content.
presentation-time|lambda-window-manager-presentation-time-demo|passive|presented|Client logs sync_output and presented feedback.
idle-inhibit|lambda-window-manager-idle-inhibit-demo|passive|active inhibitor|Idle inhibitor window appears; desktop should not blank while active.
relative-pointer|lambda-window-manager-relative-pointer-demo|interactive|move pointer|Move over the window; relative motion events are logged.
pointer-constraints|lambda-window-manager-pointer-constraints-demo|interactive|move pointer|Move over the window; lock/unlock events do not lose cursor permanently.
clipboard|lambda-window-manager-clipboard-demo|interactive|received|Click the window; it should set and receive clipboard text.
primary-selection|lambda-window-manager-primary-selection-demo|interactive|received|Click/select in the window; it should receive primary selection text.
dnd|lambda-window-manager-dnd-demo|interactive|target received|Drag orange source to blue target; target receives the payload.
popup|lambda-window-manager-popup-demo|interactive|popup configured|Popup appears; hover/click rows and click outside to dismiss.
popup-grab|lambda-window-manager-popup-grab-demo|interactive|requested menu popup|Click parent, hover More >, click outside; requires popup_grabs config.
activation|lambda-window-manager-activation-demo|passive|activating second window|Second window is raised/focused after activation request.
EOF
}

list_demos() {
  demo_table | while IFS='|' read -r name target mode needle expected; do
    printf "%-18s %-48s %-11s %s\n" "$name" "$target" "$mode" "$expected"
  done
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --no-build)
      BUILD=0
      shift
      ;;
    --include-popup-grab)
      INCLUDE_POPUP_GRAB=1
      shift
      ;;
    --passive-only)
      PASSIVE_ONLY=1
      shift
      ;;
    --demo)
      SELECTED_DEMO="${2:-}"
      if [[ -z "$SELECTED_DEMO" ]]; then
        echo "--demo requires a name" >&2
        exit 2
      fi
      shift 2
      ;;
    --seconds)
      SECONDS_PER_DEMO="${2:-}"
      if ! [[ "$SECONDS_PER_DEMO" =~ ^[0-9]+$ ]] || [[ "$SECONDS_PER_DEMO" -le 0 ]]; then
        echo "--seconds requires a positive integer" >&2
        exit 2
      fi
      shift 2
      ;;
    --list)
      list_demos
      exit 0
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

if [[ -z "${WAYLAND_DISPLAY:-}" ]]; then
  display_file="${XDG_RUNTIME_DIR:-}/lambda-window-manager-display"
  if [[ -r "$display_file" ]]; then
    export WAYLAND_DISPLAY="$(cat "$display_file")"
  fi
fi

if [[ -z "${WAYLAND_DISPLAY:-}" ]]; then
  echo "WAYLAND_DISPLAY is not set and lambda-window-manager-display was not found." >&2
  echo "Start lambda-window-manager first, then rerun this script from another TTY or SSH shell." >&2
  exit 1
fi

mapfile -t rows < <(demo_table)
targets=()
for row in "${rows[@]}"; do
  IFS='|' read -r name target mode needle expected <<<"$row"
  if [[ "$name" == "popup-grab" && "$INCLUDE_POPUP_GRAB" -ne 1 && "$SELECTED_DEMO" != "popup-grab" ]]; then
    continue
  fi
  if [[ "$PASSIVE_ONLY" -eq 1 && "$mode" != "passive" ]]; then
    continue
  fi
  if [[ -n "$SELECTED_DEMO" && "$SELECTED_DEMO" != "$name" ]]; then
    continue
  fi
  targets+=("$target")
done

if [[ "${#targets[@]}" -eq 0 ]]; then
  echo "No demos selected. Use --list to see valid names." >&2
  exit 2
fi

if [[ "$BUILD" -eq 1 ]]; then
  cmake --build "$BUILD_DIR" --target "${targets[@]}" -j"$(nproc)"
fi

mkdir -p "$LOG_DIR"

echo "Compositor demo run:"
echo "  WAYLAND_DISPLAY: $WAYLAND_DISPLAY"
echo "  build dir:       $BUILD_DIR"
echo "  log dir:         $LOG_DIR"
echo "  seconds/demo:    $SECONDS_PER_DEMO"
echo

failed=0
for row in "${rows[@]}"; do
  IFS='|' read -r name target mode needle expected <<<"$row"
  if [[ "$name" == "popup-grab" && "$INCLUDE_POPUP_GRAB" -ne 1 && "$SELECTED_DEMO" != "popup-grab" ]]; then
    continue
  fi
  if [[ "$PASSIVE_ONLY" -eq 1 && "$mode" != "passive" ]]; then
    continue
  fi
  if [[ -n "$SELECTED_DEMO" && "$SELECTED_DEMO" != "$name" ]]; then
    continue
  fi

  exe="$BUILD_DIR/$target"
  log="$LOG_DIR/$name.log"
  if [[ ! -x "$exe" ]]; then
    echo "FAIL $name: missing executable $exe"
    failed=1
    continue
  fi

  echo "== $name =="
  echo "Expected: $expected"
  if [[ "$mode" == "interactive" ]]; then
    echo "Action: perform the interaction during the ${SECONDS_PER_DEMO}s window."
  fi

  set +e
  timeout --signal=INT "${SECONDS_PER_DEMO}s" "$exe" >"$log" 2>&1
  status=$?
  set -e

  if [[ "$status" -ne 0 && "$status" -ne 124 && "$status" -ne 130 ]]; then
    echo "FAIL $name: exited with status $status; see $log"
    failed=1
  else
    if [[ -n "$needle" ]] && ! grep -Fqi "$needle" "$log"; then
      echo "CHECK $name: expected log text not found: $needle"
      echo "       see $log"
      failed=1
    else
      echo "OK   $name: log check passed"
    fi
  fi
  echo
done

echo "Logs written to: $LOG_DIR"
if [[ "$failed" -ne 0 ]]; then
  echo "One or more demos need inspection. This does not replace the visual checks in docs/compositor-testing.md."
  exit 1
fi

echo "All selected demo log checks passed. Confirm the visual behavior before marking the readiness item done."
