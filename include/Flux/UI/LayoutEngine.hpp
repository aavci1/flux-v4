#pragma once

#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/TextLayoutOptions.hpp>

#include <limits>
#include <optional>

namespace flux {

struct LayoutConstraints {
  float maxWidth = std::numeric_limits<float>::infinity();
  float maxHeight = std::numeric_limits<float>::infinity();
  float minWidth = 0.f;
  float minHeight = 0.f;
  /// Set by `HStack` for each row child: vertical alignment of the row (`HStack::vAlign`).
  /// `Rectangle` with an explicit frame uses `resolveRectangleBounds` (`LeafBounds.hpp`) to
  /// vertically center (etc.) in the row cell. Other leaves typically fill row height; this flag
  /// does not apply to them. Parent stacks (e.g. `VStack`) clear it when building children so it
  /// does not leak into nested rows.
  std::optional<VerticalAlignment> hStackCrossAlign;
  /// Set by `VStack` / `ForEach` for each row: horizontal alignment of content in the column slot.
  /// The row frame is always full `innerW` so nested flex (e.g. `HStack` + `Spacer`) does not
  /// overflow. `Text` applies this via `TextLayoutOptions` in `Element.cpp` (glyph alignment);
  /// fixed-size `Rectangle` uses `resolveRectangleBounds` when the laid-out child is wider than the
  /// slot. Cleared by `HStack`, `Grid`, `OffsetView`, and `ZStack` (fresh constraints) so it does not leak.
  std::optional<HorizontalAlignment> vStackCrossAlign;
};

class LayoutEngine {
public:
  /// Clears the current child frame. Call at the start of each full `build` pass so the root
  /// does not read a stale `childFrame` left from the previous pass (e.g. after resize).
  void resetForBuild();

  void setChildFrame(Rect frame);
  Rect childFrame() const;

private:
  Rect childFrame_{};
};

} // namespace flux
