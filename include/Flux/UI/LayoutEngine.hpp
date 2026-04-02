#pragma once

/// \file Flux/UI/LayoutEngine.hpp
///
/// Part of the Flux public API.


#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/TextLayoutOptions.hpp>

#include <limits>
#include <optional>

namespace flux {

/// Cross-axis alignment propagated by stacks — not size constraints; carried beside
/// \ref LayoutConstraints via \ref BuildContext::hints().
struct LayoutHints {
  /// Set by `HStack` for each row child (`HStack::vAlign`). Used by `resolveLeafLayoutBounds` and
  /// similar. Cleared by `VStack` when building children so it does not leak into nested rows.
  std::optional<VerticalAlignment> hStackCrossAlign;
  /// Set by `VStack` / `ForEach` for each row (`VStack::hAlign`). `Text` applies via
  /// `TextLayoutOptions` in `Element.cpp`. Cleared by `HStack`, `Grid`, `OffsetView`, `ZStack`.
  std::optional<HorizontalAlignment> vStackCrossAlign;
};

struct LayoutConstraints {
  float maxWidth = std::numeric_limits<float>::infinity();
  float maxHeight = std::numeric_limits<float>::infinity();
  float minWidth = 0.f;
  float minHeight = 0.f;
};

class LayoutEngine {
public:
  /// Clears the current child frame. Call at the start of each full `build` pass so the root
  /// does not read a stale `childFrame` left from the previous pass (e.g. after resize).
  void resetForBuild();

  /// Parent assigns the rectangle for the current child before building it.
  void setChildFrame(Rect frame);

  /// Read the frame assigned by the parent for this node. Debug builds assert that
  /// \ref setChildFrame was called since the last consume (per-child contract).
  Rect consumeAssignedFrame();

  /// Last assigned frame without consuming the validity flag (e.g. \ref BuildOrchestrator::buildSlotRect
  /// after a full rebuild).
  Rect lastAssignedFrame() const;

  /// \deprecated Prefer \ref consumeAssignedFrame or \ref lastAssignedFrame.
  Rect childFrame() const { return lastAssignedFrame(); }

private:
  Rect childFrame_{};
#ifndef NDEBUG
  bool frameValid_{false};
#endif
};

} // namespace flux
