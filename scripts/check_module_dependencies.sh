#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

check_module() {
  local name="$1"
  local path="$2"
  local allowed="$3"
  local violations

  violations="$(grep -rh '^#include <Flux/' "$path" | grep -v "$allowed" || true)"
  if [[ -n "$violations" ]]; then
    printf '%s module has upward or cross-layer includes:\n%s\n' "$name" "$violations" >&2
    return 1
  fi
}

check_module "Detail" "include/Flux/Detail/" 'Flux/Detail/'
check_module "Core" "include/Flux/Core/" 'Flux/Core/\|Flux/Detail/'
check_module "Reactive" "include/Flux/Reactive/" 'Flux/Core/\|Flux/Detail/\|Flux/Reactive/'
check_module "Graphics" "include/Flux/Graphics/" 'Flux/Core/\|Flux/Detail/\|Flux/Graphics/'
check_module "SceneGraph" "include/Flux/SceneGraph/" 'Flux/Core/\|Flux/Reactive/\|Flux/Graphics/\|Flux/Detail/\|Flux/SceneGraph/'
check_module "Layout" "include/Flux/Layout/" 'Flux/Core/\|Flux/SceneGraph/\|Flux/Detail/\|Flux/Layout/'
