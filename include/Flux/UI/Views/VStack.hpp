#pragma once

/// \file Flux/UI/Views/VStack.hpp
///
/// Part of the Flux public API.


#include <Flux/UI/Alignment.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/ViewModifiers.hpp>

#include <vector>

namespace flux {

/// Vertical stack. Use **`.clipContent(bool)`** on the wrapping `Element` to clip children to bounds.
struct VStack : ViewModifiers<VStack> {
  Size measure(MeasureContext&, LayoutConstraints const&, LayoutHints const&, TextSystem&) const;

  float spacing = 8.f;
  /// Cross-axis alignment (horizontal in a `VStack`).
  Alignment alignment = Alignment::Center;
  /// Main-axis distribution, similar to CSS `justify-content`.
  JustifyContent justifyContent = JustifyContent::Start;
  std::vector<Element> children;

  bool operator==(VStack const& other) const {
    return spacing == other.spacing && alignment == other.alignment &&
           justifyContent == other.justifyContent &&
           elementsStructurallyEqual(children, other.children);
  }
};

} // namespace flux
