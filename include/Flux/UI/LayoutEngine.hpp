#pragma once

#include <Flux/Core/Types.hpp>

#include <limits>

namespace flux {

class Element;
class TextSystem;

struct LayoutConstraints {
  float maxWidth = std::numeric_limits<float>::infinity();
  float maxHeight = std::numeric_limits<float>::infinity();
};

class LayoutEngine {
public:
  Size measure(Element const& element, LayoutConstraints const& constraints, TextSystem& textSystem) const;

  void setChildFrame(Rect frame);
  Rect childFrame() const;

private:
  Rect childFrame_{};
};

} // namespace flux
