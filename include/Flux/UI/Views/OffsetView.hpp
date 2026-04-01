#pragma once

/// \file Flux/UI/Views/OffsetView.hpp
///
/// Part of the Flux public API.


#include <Flux/Core/Types.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/Hooks.hpp>

#include <vector>

namespace flux {

enum class ScrollAxis { Vertical, Horizontal, Both };

/// Internal: applies a translation to scroll content. Used by `ScrollView`.
/// Children are stacked along the scroll axis with no extra gap between siblings; use a single
/// `VStack`/`HStack` (or similar) as the child when you need spacing.
struct OffsetView {
  Point offset{};
  ScrollAxis axis = ScrollAxis::Vertical;
  State<Size> viewportSize{};
  State<Size> contentSize{};
  float flexGrow = 0.f;
  float flexShrink = 0.f;
  float minSize = 0.f;
  std::vector<Element> children;
};

} // namespace flux
