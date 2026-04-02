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

namespace flux::views {

/// Image view component. `source` references `flux::Image` (bitmap); distinct from this `Image` view type.
/// Use \ref Element modifiers for interaction, size, opacity, and rounded corners.
struct Image : ViewModifiers<Image> {
  static constexpr bool memoizable = true;

  void build(::flux::BuildContext&) const;
  ::flux::Size measure(::flux::BuildContext&, ::flux::LayoutConstraints const&, ::flux::LayoutHints const&,
                       ::flux::TextSystem&) const;

  std::shared_ptr<flux::Image> source;
  ImageFillMode fillMode = ImageFillMode::Cover;
};

} // namespace flux::views
