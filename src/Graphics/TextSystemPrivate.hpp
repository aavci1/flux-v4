#pragma once

/// \file TextSystemPrivate.hpp
///
/// Internal helpers shared by \c TextSystem.cpp and \c CoreTextSystem.mm (not public API).

#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/TextLayout.hpp>
#include <Flux/Graphics/TextLayoutOptions.hpp>

#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace flux::detail {

void normalizeOriginsToTopLeft(TextLayout& layout);

void applyBoxOptions(TextLayout& layout, Rect const& box, TextLayoutOptions const& options);

/// Deep structural compare for paragraph-cache testing / parallel assert. Optional \p dumpOut on mismatch.
bool paragraphCacheLayoutsStructurallyEqual(TextLayout const& a, TextLayout const& b,
                                          std::string* dumpOut = nullptr);

/// OpenType glyph id 0 is `.notdef`. \c CTRunGetGlyphs may still return it while \c CTLineDraw skips it
/// visually; we filter at storage time so runs match what is drawn. The first kept glyph anchors the run;
/// positions are stored relative to that anchor. Call \c CTRunGetTypographicBounds over
/// \c CFRange{ firstKeptIndex, glyphCount - firstKeptIndex } so bounds exclude empty leading space from
/// leading `.notdef` glyphs.
std::vector<std::size_t> filterDrawableGlyphs(std::span<std::uint16_t const> gids);

} // namespace flux::detail
