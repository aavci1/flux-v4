#pragma once

/// \file Flux/UI/Views/Spacer.hpp
///
/// Flexible empty region for HStack/VStack: expands along the stack main axis.
/// Use chained `.flex(...)` to override grow/shrink and, when needed, explicit flex-basis.

#include <Flux/UI/Detail/PrimitiveForwards.hpp>
#include <Flux/UI/ViewModifiers.hpp>

#include <cstdint>
#include <memory>

namespace flux {

struct Spacer : ViewModifiers<Spacer> {
  Size measure(MeasureContext&, LayoutConstraints const&, LayoutHints const&, TextSystem&) const;

  constexpr bool operator==(Spacer const&) const noexcept { return true; }
};

} // namespace flux
