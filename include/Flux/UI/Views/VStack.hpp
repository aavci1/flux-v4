#pragma once

/// \file Flux/UI/Views/VStack.hpp
///
/// Part of the Flux public API.


#include <Flux/Graphics/TextLayoutOptions.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/ViewModifiers.hpp>

#include <vector>

namespace flux {

/// Vertical stack. Use **`.clipContent(bool)`** on the wrapping `Element` to clip children to bounds.
struct VStack : ViewModifiers<VStack> {
  float spacing = 8.f;
  HorizontalAlignment hAlign = HorizontalAlignment::Leading;
  std::vector<Element> children;
};

} // namespace flux
