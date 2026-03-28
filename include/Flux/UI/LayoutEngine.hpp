#pragma once

#include <Flux/Core/Types.hpp>

#include <limits>

namespace flux {

class BuildContext;
class Element;
class TextSystem;

struct LayoutConstraints {
  float maxWidth = std::numeric_limits<float>::infinity();
  float maxHeight = std::numeric_limits<float>::infinity();
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
