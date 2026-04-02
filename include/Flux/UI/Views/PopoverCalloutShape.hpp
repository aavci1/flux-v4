#pragma once

/// \file Flux/UI/Views/PopoverCalloutShape.hpp
///
/// Part of the Flux public API.


#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/Styles.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/Views/PopoverPlacement.hpp>
#include <Flux/UI/Views/Rectangle.hpp>

#include <optional>

namespace flux {

/// Single filled/stroked path: rounded card + optional callout triangle (merged outline).
struct PopoverCalloutShape : ViewModifiers<PopoverCalloutShape> {
  PopoverPlacement placement = PopoverPlacement::Below;
  bool arrow = true;
  float padding = 12.f;
  CornerRadius cornerRadius{10.f};
  Color backgroundColor = Color::hex(0xFFFFFF);
  Color borderColor = Color::hex(0xE0E0E6);
  float borderWidth = 1.f;
  std::optional<Size> maxSize{};
  Element content{Rectangle{}};

  static constexpr float kArrowW = 16.f;
  static constexpr float kArrowH = 8.f;
};

} // namespace flux
