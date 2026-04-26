# Flux v5 Implementation Plan

Status: implemented through Stage 8; Stage 9 release prep is in progress.

## Strategy

Flux v5 is a hard cutover to a retained mount runtime with fine-grained reactivity. The public surface is the current v5 API under `Flux/Reactive`, `Flux/UI`, and `Flux/SceneGraph`.

## Architecture

- View bodies describe a mount-time retained scene tree.
- `MountRoot` owns the root scope and scene-node subtree.
- `MountContext` carries the owner scope, environment stack, text system, and root invalidation hooks during mount.
- `Signal`, `Computed`, `Effect`, and `Scope` form the reactive graph.
- `Bindable<T>` modifier values install effects that update mounted scene nodes.
- `For`, `Show`, and `Switch` own row and branch scopes for reactive control flow.
- `EnvironmentLayer` supports both constant values and reactive signals.
- `Window` owns a reactive `Theme` signal and exposes `setTheme()`.

## Stage Plan

| Stage | Goal | Status |
|-------|------|--------|
| 0 | Branch and scaffolding | Done |
| 1 | Standalone reactive prototype | Done |
| 2 | In-tree reactive core | Done |
| 3 | Element and bindable refactor | Done |
| 4 | Mount root and scope tree | Done |
| 5 | Hooks rewrite | Done |
| 6 | Reactive control flow | Done |
| 7 | Theme and environment reactivity | Done |
| 8 | Examples migration | Done |
| 9 | Cleanup, final validation, release prep | In progress |

## Stage 9 Gate

- Public reactive headers and tests use the final `Flux/Reactive` paths.
- All examples build and launch-smoke.
- Normal and sanitizer test builds are green.
- Public docs describe the v5 runtime and migration path.
- Final performance data is recorded in `docs/v5-final-perf.md`.
- Main-branch promotion and tags require explicit release approval.
