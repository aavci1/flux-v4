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
struct OffsetView : ViewModifiers<OffsetView> {
  Size measure(MeasureContext&, LayoutConstraints const&, LayoutHints const&, TextSystem&) const;
  std::unique_ptr<scenegraph::SceneNode> mount(MountContext&) const;

  Point offset{};
  ScrollAxis axis = ScrollAxis::Vertical;
  Signal<Size> viewportSize{};
  Signal<Size> contentSize{};
  std::vector<Element> children;

  bool operator==(OffsetView const& other) const {
    return offset == other.offset && axis == other.axis &&
           viewportSize == other.viewportSize && contentSize == other.contentSize &&
           elementsStructurallyEqual(children, other.children);
  }
};

} // namespace flux
