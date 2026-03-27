#pragma once

#include <Flux/Graphics/AttributedString.hpp>
#include <Flux/Graphics/TextLayout.hpp>

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

namespace flux {

class TextSystem {
public:
  virtual ~TextSystem() = default;

  virtual std::shared_ptr<TextLayout> shape(AttributedString const& text, float maxWidth = 0.f) = 0;

  virtual std::shared_ptr<TextLayout> shapePlain(std::string_view utf8, TextAttribute const& attr,
                                                 float maxWidth = 0.f) = 0;

  virtual Size measure(AttributedString const& text, float maxWidth = 0.f) = 0;

  virtual Size measurePlain(std::string_view utf8, TextAttribute const& attr, float maxWidth = 0.f) = 0;

  virtual std::uint32_t resolveFontId(std::string_view fontFamily, float weight, bool italic) = 0;

  virtual std::vector<std::uint8_t> rasterizeGlyph(std::uint32_t fontId, std::uint16_t glyphId, float size,
                                                  std::uint32_t& outWidth, std::uint32_t& outHeight,
                                                  Point& outBearing) = 0;
};

} // namespace flux
