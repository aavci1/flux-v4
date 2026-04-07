#pragma once

/// \file TextSystemPrivate.hpp
///
/// Internal helpers shared by \c TextSystem.cpp and \c CoreTextSystem.mm (not public API).

#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/TextLayout.hpp>
#include <Flux/Graphics/TextLayoutOptions.hpp>

namespace flux::detail {

void normalizeOriginsToTopLeft(TextLayout& layout);

void applyBoxOptions(TextLayout& layout, Rect const& box, TextLayoutOptions const& options);

} // namespace flux::detail
