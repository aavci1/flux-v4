#pragma once

#include <Flux/Graphics/TextLayoutOptions.hpp>
#include <Flux/UI/Element.hpp>

#include <vector>

namespace flux {

struct ZStack {
  HorizontalAlignment hAlign = HorizontalAlignment::Center;
  VerticalAlignment vAlign = VerticalAlignment::Center;
  bool clip = false;
  std::vector<Element> children;
};

} // namespace flux
