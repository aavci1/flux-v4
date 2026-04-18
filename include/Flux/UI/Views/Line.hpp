#pragma once

/// \file Flux/UI/Views/Line.hpp
///
/// Part of the Flux public API.


#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/Styles.hpp>
#include <Flux/UI/Detail/PrimitiveForwards.hpp>
#include <Flux/UI/ViewModifiers.hpp>

namespace flux {

struct Line : ViewModifiers<Line> {
  static constexpr bool memoizable = true;

  void layout(LayoutContext&) const;
  Size measure(MeasureContext&, LayoutConstraints const&, LayoutHints const&, TextSystem&) const;
  [[nodiscard]] std::uint64_t measureCacheKey() const noexcept;

  Point from{};
  Point to{};
  StrokeStyle stroke{};
};

} // namespace flux
