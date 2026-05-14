#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

check_module() {
  local name="$1"
  local path="$2"
  local allowed="$3"
  local violations

  violations="$(grep -Rnh '^#include <Flux/' "$path" | grep -v "$allowed" || true)"
  if [[ -n "$violations" ]]; then
    printf '%s module has upward or cross-layer includes:\n%s\n' "$name" "$violations" >&2
    return 1
  fi
}

check_source_module() {
  local name="$1"
  local path="$2"
  local allowed="$3"
  local violations

  if [[ ! -d "$path" ]]; then
    return 0
  fi

  violations="$(
    grep -RnE '^#include[[:space:]]+[<"]((Flux/)?(Detail|Core|Reactive|Graphics|SceneGraph|Layout|UI)/)' "$path" |
      grep -Ev "$allowed" || true
  )"
  if [[ -n "$violations" ]]; then
    printf '%s sources have upward or cross-layer includes:\n%s\n' "$name" "$violations" >&2
    return 1
  fi
}

check_module "Detail" "include/Flux/Detail/" 'Flux/Detail/'
check_module "Core" "include/Flux/Core/" 'Flux/Core/\|Flux/Detail/'
check_module "Reactive" "include/Flux/Reactive/" 'Flux/Core/\|Flux/Detail/\|Flux/Reactive/'
check_module "Graphics" "include/Flux/Graphics/" 'Flux/Core/\|Flux/Detail/\|Flux/Graphics/'
check_module "SceneGraph" "include/Flux/SceneGraph/" 'Flux/Core/\|Flux/Reactive/\|Flux/Graphics/\|Flux/Detail/\|Flux/SceneGraph/'
check_module "Layout" "include/Flux/Layout/" 'Flux/Core/\|Flux/SceneGraph/\|Flux/Detail/\|Flux/Layout/'

check_source_module "Core" "src/Core/" '(<Flux/(Core|Detail)/)|"(Core|Detail)/'
check_source_module "Reactive" "src/Reactive/" '(<Flux/(Core|Detail|Reactive)/)|"(Core|Detail|Reactive)/'
check_source_module "Graphics" "src/Graphics/" '(<Flux/(Core|Detail|Graphics)/)|"(Core|Detail|Graphics)/'
check_source_module "SceneGraph" "src/SceneGraph/" '(<Flux/(Core|Reactive|Graphics|Detail|SceneGraph)/)|"(Core|Reactive|Graphics|Detail|SceneGraph)/'
check_source_module "Layout" "src/Layout/" '(<Flux/(Core|SceneGraph|Detail|Layout)/)|"(Core|SceneGraph|Detail|Layout)/'
