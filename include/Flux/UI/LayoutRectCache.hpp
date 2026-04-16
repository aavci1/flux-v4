#pragma once

/// \file Flux/UI/LayoutRectCache.hpp
///
/// Part of the Flux public API.


#include <Flux/UI/ComponentKey.hpp>
#include <Flux/Core/Types.hpp>
#include <Flux/UI/LayoutContext.hpp>
#include <Flux/UI/LayoutTree.hpp>

#include <optional>
#include <unordered_map>

namespace flux {

class StateStore;

/// Two-generation layout rect cache. Filled at the end of each rebuild pass.
/// Components read their own rect via `forCurrentComponent()`; overlays and
/// popovers read anchor rects via `forLeafKeyPrefix()`.
class LayoutRectCache {
public:
  /// Called at end of `BuildOrchestrator::rebuild()`. Swaps generations and
  /// fills `current_` from `ctx.subtreeRootLayouts()` and `tree`.
  /// Each entry uses the subtree root node's \ref LayoutNode::worldBounds (the
  /// composite container's allotted frame), not a union of all descendants — a
  /// union can exceed the container when children overflow (e.g. flex rows).
  void fill(LayoutTree const& tree, LayoutContext const& ctx);
  void fill(LayoutTree const& tree, LayoutContext::SubtreeRootMap const& roots);

  /// Rect for the composite currently executing `body()`.
  /// Returns nullopt when called outside a build pass.
  std::optional<Rect> forCurrentComponent(StateStore const& store) const;

  /// Rect for a specific composite key. Used by `useLayoutRect()` when given
  /// a captured key from a prior `body()` call.
  std::optional<Rect> forKey(ComponentKey const& key) const;

  /// Longest-prefix rect for a leaf `stableTargetKey`. Used by popover anchor
  /// tracking and `forTapAnchor`.
  std::optional<Rect> forLeafKeyPrefix(ComponentKey const& stableTargetKey) const;

  /// During `onTap`: rect for the innermost composite ancestor of the tapped
  /// node. `tapLeafKey` must be the leaf `stableTargetKey` set before `onTap`
  /// fires; empty otherwise.
  std::optional<Rect> forTapAnchor(ComponentKey const& tapLeafKey) const;

private:
  std::unordered_map<ComponentKey, Rect, ComponentKeyHash> current_{};
  std::unordered_map<ComponentKey, Rect, ComponentKeyHash> prev_{};
};

} // namespace flux
