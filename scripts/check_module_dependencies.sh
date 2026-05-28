#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

check_module() {
  local name="$1"
  local path="$2"
  local allowed="$3"
  local violations

  violations="$(grep -Rnh '^#include <Lambda/' "$path" | grep -v "$allowed" || true)"
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
    grep -RnE '^#include[[:space:]]+[<"]((Lambda/)?(Detail|Core|Reactive|Graphics|SceneGraph|Layout|UI)/)' "$path" |
      grep -Ev "$allowed" || true
  )"
  if [[ -n "$violations" ]]; then
    printf '%s sources have upward or cross-layer includes:\n%s\n' "$name" "$violations" >&2
    return 1
  fi
}

check_module "Detail" "include/Lambda/Detail/" 'Lambda/Detail/'
check_module "Core" "include/Lambda/Core/" 'Lambda/Core/\|Lambda/Detail/'
check_module "Reactive" "include/Lambda/Reactive/" 'Lambda/Core/\|Lambda/Detail/\|Lambda/Reactive/'
check_module "Graphics" "include/Lambda/Graphics/" 'Lambda/Core/\|Lambda/Detail/\|Lambda/Graphics/'
check_module "SceneGraph" "include/Lambda/SceneGraph/" 'Lambda/Core/\|Lambda/Reactive/\|Lambda/Graphics/\|Lambda/Detail/\|Lambda/SceneGraph/'
check_module "Layout" "include/Lambda/Layout/" 'Lambda/Core/\|Lambda/SceneGraph/\|Lambda/Detail/\|Lambda/Layout/'

check_source_module "Core" "src/Core/" '(<Lambda/(Core|Detail)/)|"(Core|Detail)/'
check_source_module "Reactive" "src/Reactive/" '(<Lambda/(Core|Detail|Reactive)/)|"(Core|Detail|Reactive)/'
check_source_module "Graphics" "src/Graphics/" '(<Lambda/(Core|Detail|Graphics)/)|"(Core|Detail|Graphics)/'
check_source_module "SceneGraph" "src/SceneGraph/" '(<Lambda/(Core|Reactive|Graphics|Detail|SceneGraph)/)|"(Core|Reactive|Graphics|Detail|SceneGraph)/'
check_source_module "Layout" "src/Layout/" '(<Lambda/(Core|SceneGraph|Detail|Layout)/)|"(Core|SceneGraph|Detail|Layout)/'
