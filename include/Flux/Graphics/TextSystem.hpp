#pragma once

#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/AttributedString.hpp>
#include <Flux/Graphics/TextLayout.hpp>
#include <Flux/Graphics/TextLayoutOptions.hpp>

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

namespace flux {

class TextSystem {
public:
  virtual ~TextSystem() = default;

  // -----------------------------------------------------------------
  // Layout — returns a TextLayout with PlacedRun origins in layout space
  // -----------------------------------------------------------------

  /// Unconstrained: no box, no alignment. `maxWidth == 0` means no wrapping.
  virtual std::shared_ptr<TextLayout> layout(AttributedString const& text, float maxWidth = 0.f,
                                             TextLayoutOptions const& options = {}) = 0;

  virtual std::shared_ptr<TextLayout> layout(std::string_view utf8, TextAttribute const& attr, float maxWidth = 0.f,
                                             TextLayoutOptions const& options = {}) = 0;

  /// Box-constrained: PlacedRun origins are pre-offset for alignment within the box.
  /// Drawing: `canvas.drawTextLayout(*result, {box.x, box.y})` — no further arithmetic.
  std::shared_ptr<TextLayout> layout(AttributedString const& text, Rect const& box,
                                     TextLayoutOptions const& options = {});

  std::shared_ptr<TextLayout> layout(std::string_view utf8, TextAttribute const& attr, Rect const& box,
                                     TextLayoutOptions const& options = {});

  // -----------------------------------------------------------------
  // Measure — CPU only, safe in layout pass, no canvas required
  // -----------------------------------------------------------------

  virtual Size measure(AttributedString const& text, float maxWidth = 0.f,
                       TextLayoutOptions const& options = {}) = 0;

  virtual Size measure(std::string_view utf8, TextAttribute const& attr, float maxWidth = 0.f,
                       TextLayoutOptions const& options = {}) = 0;

  // -----------------------------------------------------------------
  // Backend interface (called by GlyphAtlas — not for app code)
  // -----------------------------------------------------------------

  virtual std::uint32_t resolveFontId(std::string_view fontFamily, float weight, bool italic) = 0;

  virtual std::vector<std::uint8_t> rasterizeGlyph(std::uint32_t fontId, std::uint16_t glyphId, float size,
                                                   std::uint32_t& outWidth, std::uint32_t& outHeight,
                                                   Point& outBearing) = 0;
};

} // namespace flux
