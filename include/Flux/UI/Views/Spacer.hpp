#pragma once

/// \file Flux/UI/Views/Spacer.hpp
///
/// Flexible empty region for HStack/VStack: expands along the stack main axis.

#include <Flux/UI/Detail/PrimitiveForwards.hpp>
#include <Flux/UI/ViewModifiers.hpp>

namespace flux {

struct Spacer : ViewModifiers<Spacer> {
  static constexpr bool memoizable = true;

  void build(BuildContext&) const;
  Size measure(BuildContext&, LayoutConstraints const&, LayoutHints const&, TextSystem&) const;

  /// Minimum size on the stack main axis before flex distribution.
  float minLength = 0.f;
  float flexGrow = 1.f;
  float flexShrink = 0.f;
};

} // namespace flux
