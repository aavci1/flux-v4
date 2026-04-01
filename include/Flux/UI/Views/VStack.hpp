#pragma once

/// \file Flux/UI/Views/VStack.hpp
///
/// Part of the Flux public API.


#include <Flux/Graphics/TextLayoutOptions.hpp>
#include <Flux/UI/Element.hpp>

#include <vector>

namespace flux {

struct VStack {
  float spacing = 8.f;
  float padding = 0.f;
  HorizontalAlignment hAlign = HorizontalAlignment::Leading;
  bool clip = false;
  std::vector<Element> children;
};

} // namespace flux
