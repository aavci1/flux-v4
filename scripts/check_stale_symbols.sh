#!/usr/bin/env bash

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

failures=()

add_failure() {
  failures+=("$1")
}

is_allowed_header_only_class() {
  case "$1" in
    AnimationBase|Bindable|Clipboard|Computed|Effect|EnvironmentLayer|EnvironmentReadTrackingScope|EnvironmentValue|ForView|HookInteractionSignalScope|HookLayoutScope|PreparedRenderOps|Renderer|Scope|ScopedEnvironmentSnapshot|ScopedTimer|ShowView|Signal|SmallFn|SmallVector|SwitchView)
      return 0
      ;;
  esac
  return 1
}

is_allowed_forward_without_direct_instantiation() {
  case "$1" in
    GestureTracker|PlatformWindow|PreparedRenderOps|Renderer|TextSystem)
      return 0
      ;;
  esac
  return 1
}

has_definition() {
  local type="$1"
  rg -q "(^|[[:space:]])(class|struct)[[:space:]]+${type}([^A-Za-z0-9_]|$).*\\{" include src
}

has_direct_instantiation() {
  local type="$1"
  rg -q "make_unique<${type}\\b|make_shared<${type}\\b|new[[:space:]]+${type}\\b|:[[:space:]]+${type}\\b|[[:space:]]${type}\\{|[[:space:]]${type}\\(" include src
}

has_out_of_class_implementation() {
  local type="$1"
  rg -q "(^|[[:space:]~:])${type}::" include src
}

for stale_path in \
  "include/Flux/UI/SceneBuilder.hpp" \
  "src/UI/SceneBuilder/MeasureLayoutCache.hpp" \
  "src/UI/SceneBuilder" \
  "tests/SceneBuilderLayoutTests.cpp" \
  "tests/SceneBuilderReuseTests.cpp" \
  "tests/SceneBuilderTestSupport.hpp" \
  "tests/SceneGeometryIndexTests.cpp" \
  "tests/SemanticThemeTests.cpp"; do
  if [[ -e "$stale_path" ]]; then
    add_failure "removed SceneBuilder artifact still exists: $stale_path"
  fi
done

if rg -n "MeasureLayoutCache|SceneBuilder|SceneBuilderTestSupport|include/Flux/UI/SceneBuilder\\.hpp|src/UI/SceneBuilder" include src tests examples CMakeLists.txt README.md >/tmp/flux-stale-symbols.$$ 2>/dev/null; then
  while IFS= read -r line; do
    add_failure "stale SceneBuilder/MeasureLayoutCache reference: $line"
  done </tmp/flux-stale-symbols.$$
fi
rm -f /tmp/flux-stale-symbols.$$

while IFS= read -r source; do
  if ! rg -q "$(basename "$source")" CMakeLists.txt; then
    add_failure "implementation source is not listed in CMakeLists.txt: $source"
  fi
done < <(find src -type f \( -name '*.c' -o -name '*.cpp' -o -name '*.mm' \) | sort)

while IFS= read -r header; do
  while IFS= read -r type; do
    [[ -z "$type" ]] && continue
    if has_out_of_class_implementation "$type"; then
      continue
    fi
    if is_allowed_header_only_class "$type"; then
      continue
    fi
    add_failure "class $type declared in $header has no out-of-class implementation; mark it header-only or remove the stale declaration"
  done < <(
    grep -E '^[[:space:]]*class[[:space:]]+[A-Z][A-Za-z0-9_]+' "$header" 2>/dev/null \
      | grep -Ev ';[[:space:]]*$' \
      | sed -E 's/^[[:space:]]*class[[:space:]]+([A-Z][A-Za-z0-9_]+).*/\1/'
  )
done < <(find include -name '*.hpp' -type f | sort)

while IFS= read -r line; do
  type="$(printf '%s\n' "$line" | sed -E 's/.*class[[:space:]]+([A-Z][A-Za-z0-9_]+)[[:space:]]*;.*/\1/')"
  [[ -z "$type" ]] && continue
  if ! has_definition "$type"; then
    add_failure "forward declaration has no matching definition: $line"
    continue
  fi
  if has_direct_instantiation "$type"; then
    continue
  fi
  if is_allowed_forward_without_direct_instantiation "$type"; then
    continue
  fi
  add_failure "forward declaration appears never directly instantiated: $line"
done < <(grep -RnE '^[[:space:]]*class[[:space:]]+[A-Z][A-Za-z0-9_]+[[:space:]]*;' include src 2>/dev/null)

if ((${#failures[@]} > 0)); then
  printf 'Stale symbol scan failed:\n' >&2
  for failure in "${failures[@]}"; do
    printf '  - %s\n' "$failure" >&2
  done
  exit 1
fi

printf 'Stale symbol scan passed.\n'
