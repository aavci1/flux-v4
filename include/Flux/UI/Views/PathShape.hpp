#pragma once

/// \file Flux/UI/Views/PathShape.hpp
///
/// Part of the Flux public API.


#include <Flux/Graphics/Path.hpp>
#include <Flux/UI/Detail/PrimitiveForwards.hpp>
#include <Flux/UI/ViewModifiers.hpp>

namespace flux {

/// Scene path primitive (name avoids clashing with `flux::Path`). Fill, stroke, and shadow use \ref Element
/// modifiers (\c fill, \c stroke, \c shadow).
struct PathShape : ViewModifiers<PathShape> {
  Size measure(MeasureContext&, LayoutConstraints const&, LayoutHints const&, TextSystem&) const;

  Path path{};

  bool operator==(PathShape const& other) const {
    return path == other.path;
  }
};

} // namespace flux
