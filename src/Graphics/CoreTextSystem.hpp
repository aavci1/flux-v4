#pragma once

#include <Flux/Graphics/TextSystem.hpp>

namespace flux {

class CoreTextSystem final : public TextSystem {
public:
  CoreTextSystem();
  ~CoreTextSystem() override;

  CoreTextSystem(CoreTextSystem const&) = delete;
  CoreTextSystem& operator=(CoreTextSystem const&) = delete;

  std::shared_ptr<TextLayout> shape(AttributedString const& text, float maxWidth) override;

  std::shared_ptr<TextLayout> shapePlain(std::string_view utf8, TextAttribute const& attr,
                                         float maxWidth) override;

  Size measure(AttributedString const& text, float maxWidth) override;

  Size measurePlain(std::string_view utf8, TextAttribute const& attr, float maxWidth) override;

  std::uint32_t resolveFontId(std::string_view fontFamily, float weight, bool italic) override;

  std::vector<std::uint8_t> rasterizeGlyph(std::uint32_t fontId, std::uint16_t glyphId, float size,
                                           std::uint32_t& outWidth, std::uint32_t& outHeight,
                                           Point& outBearing) override;

private:
  struct Impl;
  std::unique_ptr<Impl> d;
};

} // namespace flux
