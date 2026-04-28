#pragma once

/// \file Flux/UI/Views/HStack.hpp
///
/// Part of the Flux public API.


#include <Flux/UI/Alignment.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/ViewModifiers.hpp>

#include <vector>

namespace flux {

/// Horizontal stack. Use **`.padding(float)`** / **`.clipContent(bool)`** on the wrapping `Element` for
/// inset and clipping.
struct HStack : ViewModifiers<HStack> {
  Size measure(MeasureContext&, LayoutConstraints const&, LayoutHints const&, TextSystem&) const;

  /// Gap inserted between adjacent children on the horizontal axis.
  float spacing = 8.f;
  /// Cross-axis alignment (vertical in an `HStack`).
  Alignment alignment = Alignment::Center;
  /// Main-axis distribution, similar to CSS `justify-content`.
  JustifyContent justifyContent = JustifyContent::Start;
  /// Children laid out left-to-right.
  std::vector<Element> children;

  bool operator==(HStack const& other) const {
    return spacing == other.spacing && alignment == other.alignment &&
           justifyContent == other.justifyContent &&
           elementsStructurallyEqual(children, other.children);
  }
};

} // namespace flux
