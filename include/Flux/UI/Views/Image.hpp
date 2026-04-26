#pragma once

/// \file Flux/UI/Views/Image.hpp
///
/// Part of the Flux public API.


#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/Image.hpp>
#include <Flux/Graphics/ImageFillMode.hpp>
#include <Flux/Graphics/Styles.hpp>
#include <Flux/UI/Detail/PrimitiveForwards.hpp>
#include <Flux/UI/ViewModifiers.hpp>

#include <memory>

namespace flux {
class MountContext;
namespace scenegraph {
class SceneNode;
}
} // namespace flux

namespace flux::views {

/// Image view component. `source` references `flux::Image` (bitmap); distinct from this `Image` view type.
/// Use \ref Element modifiers for interaction, size, opacity, and rounded corners.
struct Image : ViewModifiers<Image> {
  ::flux::Size measure(::flux::MeasureContext&, ::flux::LayoutConstraints const&, ::flux::LayoutHints const&,
                       ::flux::TextSystem&) const;
  std::unique_ptr<::flux::scenegraph::SceneNode> mount(::flux::MountContext&) const;

  std::shared_ptr<flux::Image> source;
  ImageFillMode fillMode = ImageFillMode::Cover;

  bool operator==(Image const& other) const {
    return source == other.source && fillMode == other.fillMode;
  }
};

} // namespace flux::views
