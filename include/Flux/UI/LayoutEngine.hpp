#pragma once

#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/TextLayoutOptions.hpp>

#include <limits>
#include <optional>

namespace flux {

class BuildContext;
class Element;
class TextSystem;

struct LayoutConstraints {
  float maxWidth = std::numeric_limits<float>::infinity();
  float maxHeight = std::numeric_limits<float>::infinity();
  float minWidth = 0.f;
  float minHeight = 0.f;
  /// Set by `HStack` for each row child: vertical alignment of the row (`HStack::vAlign`)
  /// forwarded so cross-axis placement can be applied in leaf layout. Today only `Rectangle`
  /// (`resolveRectangleBounds` in `LeafBounds.hpp`) reads this; other leaves fill the cell
  /// height and ignore it. Parent stacks (e.g. `VStack`) clear it when building children so it
  /// does not leak into nested rows. A cross-stack layout pass could eventually replace this
  /// channel with explicit per-child frames, similar to `VStack` horizontal alignment.
  std::optional<VerticalAlignment> hStackCrossAlign;
};

class LayoutEngine {
public:
  /// Clears the current child frame. Call at the start of each full `build` pass so the root
  /// does not read a stale `childFrame` left from the previous pass (e.g. after resize).
  void resetForBuild();

  Size measure(BuildContext& ctx, Element const& element, LayoutConstraints const& constraints,
               TextSystem& textSystem) const;

  void setChildFrame(Rect frame);
  Rect childFrame() const;

private:
  Rect childFrame_{};
};

} // namespace flux
