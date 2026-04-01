#pragma once

/// \file Flux/UI/Views/HStack.hpp
///
/// Part of the Flux public API.


#include <Flux/Graphics/TextLayoutOptions.hpp>
#include <Flux/UI/Element.hpp>

#include <vector>

namespace flux {

struct HStack {
  float spacing = 8.f;
  float padding = 0.f;
  VerticalAlignment vAlign = VerticalAlignment::Center;
  bool clip = false;
  std::vector<Element> children;
};

} // namespace flux
