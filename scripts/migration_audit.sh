#!/bin/zsh

set -euo pipefail

BASE_REF="${1:-main}"
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"

cd "$ROOT_DIR"

old_pattern='SceneGraph\|NodeStore\|LayoutTree\|LayoutNode\|LayoutNodeId\|RenderContext\|RecordingCanvas\|SceneRenderer\|EventMap\|MeasureCache\|RenderLayoutTree\|LayoutRectCache\|LayoutOverlayRenderer\|renderFromLayout\|tryRetainedLayout\|canRetainedLayout\|tryCachedMeasure\|supportsIncrementalSceneReuse\|reuseSceneFromLayout\|measureCacheToken\|subtreePaintEpoch\|markPaintDirty\|layoutTree_\|layoutRects_\|layoutPins_\|layoutSubtreeRoots_\|pinGenerations_\|canReuseWholeLayout'

required_files=(
  "include/Flux/Scene/SceneNode.hpp"
  "include/Flux/Scene/ModifierSceneNode.hpp"
  "include/Flux/Scene/RectSceneNode.hpp"
  "include/Flux/Scene/TextSceneNode.hpp"
  "include/Flux/Scene/ImageSceneNode.hpp"
  "include/Flux/Scene/PathSceneNode.hpp"
  "include/Flux/Scene/LineSceneNode.hpp"
  "include/Flux/Scene/RenderSceneNode.hpp"
  "include/Flux/Scene/InteractionData.hpp"
  "include/Flux/UI/SceneBuilder.hpp"
  "src/Scene/SceneNode.cpp"
  "src/Scene/ModifierSceneNode.cpp"
  "src/Scene/RectSceneNode.cpp"
  "src/Scene/TextSceneNode.cpp"
  "src/Scene/ImageSceneNode.cpp"
  "src/Scene/PathSceneNode.cpp"
  "src/Scene/LineSceneNode.cpp"
  "src/Scene/RenderSceneNode.cpp"
  "src/UI/SceneBuilder.cpp"
)

expected_kind_lines=(
  "Group"
  "Modifier"
  "Rect"
  "Text"
  "Image"
  "Path"
  "Line"
  "Render"
  "Custom"
)

new_identifier_pattern='class SceneNode\|class ModifierSceneNode\|class RectSceneNode\|class TextSceneNode\|class ImageSceneNode\|class PathSceneNode\|class LineSceneNode\|class RenderSceneNode\|class SceneBuilder\|struct InteractionData\|SceneNodeKind'

echo "== Step 1: old identifiers are gone =="
if old_hits="$(/usr/bin/git grep -l "$old_pattern" || true)"; [[ -n "$old_hits" ]]; then
  echo "$old_hits"
  echo
  echo "FAIL: stale old-architecture identifiers still exist."
  exit 1
fi
echo "OK: no stale identifiers matched."
echo

echo "== Step 2: new identifiers exist =="
new_hits="$(/usr/bin/git grep -l "$new_identifier_pattern" -- include/Flux/Scene include/Flux/UI src/Scene src/UI || true)"
if [[ -z "$new_hits" ]]; then
  echo "FAIL: no retained-scene identifiers were found."
  exit 1
fi
printf '%s\n' "$new_hits"
echo

echo "== Step 2a: required retained-scene files =="
missing=0
for path in "${required_files[@]}"; do
  if [[ ! -f "$path" ]]; then
    echo "missing: $path"
    missing=1
  fi
done
if [[ "$missing" -ne 0 ]]; then
  echo
  echo "FAIL: missing retained-scene files."
  exit 1
fi
printf '%s\n' "${required_files[@]}"
echo

echo "== Step 2b: node file pairs =="
/bin/ls include/Flux/Scene/*SceneNode.hpp src/Scene/*SceneNode.cpp 2>/dev/null | /usr/bin/sort
echo

echo "== Step 2c: SceneNodeKind =="
kind_block="$(
  /usr/bin/awk '
    /enum class SceneNodeKind/ { in_enum = 1; next }
    in_enum && /\};/ { exit }
    in_enum { print }
  ' include/Flux/Scene/SceneNode.hpp | /usr/bin/sed 's,//.*$,,' | /usr/bin/sed 's/[[:space:]]//g' | /usr/bin/sed '/^$/d'
)"
printf '%s\n' "$kind_block"

for kind in "${expected_kind_lines[@]}"; do
  if ! printf '%s\n' "$kind_block" | /usr/bin/grep -qx "${kind},"; then
    echo
    echo "FAIL: SceneNodeKind is missing $kind."
    exit 1
  fi
done

kind_count="$(printf '%s\n' "$kind_block" | /usr/bin/wc -l | /usr/bin/tr -d ' ')"
if [[ "$kind_count" != "9" ]]; then
  echo
  echo "FAIL: SceneNodeKind expected 9 values, found $kind_count."
  exit 1
fi
echo "OK: SceneNodeKind has the expected 9 values."
echo

echo "== Step 3: line-count invariant =="
/usr/bin/git diff --stat "$BASE_REF..HEAD" -- src/Scene/ src/UI/ include/Flux/Scene/ include/Flux/UI/ || true
shortstat="$(/usr/bin/git diff --shortstat "$BASE_REF..HEAD" -- src/Scene/ src/UI/ include/Flux/Scene/ include/Flux/UI/ || true)"
echo "$shortstat"
current_total="$(
  /usr/bin/find src/Scene src/UI include/Flux/Scene include/Flux/UI \( -name '*.hpp' -o -name '*.cpp' -o -name '*.mm' \) -print0 |
    /usr/bin/xargs -0 /usr/bin/wc -l | /usr/bin/tail -1
)"
echo "current total: $current_total"

insertions="$(printf '%s\n' "$shortstat" | /usr/bin/sed -n 's/.* \([0-9][0-9]*\) insertions*(+).*/\1/p')"
deletions="$(printf '%s\n' "$shortstat" | /usr/bin/sed -n 's/.* \([0-9][0-9]*\) deletions*(-).*/\1/p')"
insertions="${insertions:-0}"
deletions="${deletions:-0}"

if (( insertions > deletions )); then
  echo
  echo "FAIL: insertions ($insertions) exceed deletions ($deletions) in core scene/ui dirs."
  exit 1
fi
echo "OK: insertions ($insertions) do not exceed deletions ($deletions)."
echo

echo "== Step 5: input path =="
platform_hits="$(/usr/bin/git grep -n 'hitTest\|EventMap' -- src/Platform include/Flux/Platform 2>/dev/null || true)"
printf '%s\n' "$platform_hits"
if printf '%s\n' "$platform_hits" | /usr/bin/grep -q 'EventMap'; then
  echo
  echo "FAIL: platform input path still mentions EventMap."
  exit 1
fi
echo "OK: platform input path no longer mentions EventMap."
echo

echo "== Step 6: invariant-test surface =="
test_hits="$(
  /bin/ls tests/*.cpp 2>/dev/null | /usr/bin/xargs /usr/bin/grep -l 'paintCache\|paintDirty\|boundsDirty\|reconcile\|hitTest\|NodeId\|SceneBuilder' 2>/dev/null || true
)"
if [[ -z "$test_hits" ]]; then
  echo "FAIL: no invariant-oriented test files found."
  exit 1
fi
printf '%s\n' "$test_hits"

echo
echo "Migration audit complete."
