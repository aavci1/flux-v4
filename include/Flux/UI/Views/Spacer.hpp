#pragma once

/// \file Flux/UI/Views/Spacer.hpp
///
/// Flexible empty region for HStack/VStack: expands along the stack main axis.
/// Use chained `.flex(...)` to override grow/shrink and, when needed, explicit flex-basis.

#include <Flux/UI/Element.hpp>
#include <Flux/UI/ViewModifiers.hpp>
#include <Flux/UI/Views/Rectangle.hpp>

namespace flux {

struct Spacer : ViewModifiers<Spacer> {
  Element body() const {
    return Element{Rectangle{}}.flex(1.f);
  }
};

} // namespace flux
