#pragma once

/// \file Flux/UI/Views/HStack.hpp
///
/// Part of the Flux public API.


#include <Flux/Graphics/TextLayoutOptions.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/ViewModifiers.hpp>

#include <vector>

namespace flux {

/// Horizontal stack. Use **`.padding(float)`** / **`.clipContent(bool)`** on the wrapping `Element` for
/// inset and clipping.
struct HStack : ViewModifiers<HStack> {
  void build(BuildContext&) const;
  Size measure(BuildContext&, LayoutConstraints const&, LayoutHints const&, TextSystem&) const;

  float spacing = 8.f;
  VerticalAlignment vAlign = VerticalAlignment::Center;
  std::vector<Element> children;
};

} // namespace flux
