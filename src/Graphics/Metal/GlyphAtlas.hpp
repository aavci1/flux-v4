#pragma once

#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/TextSystem.hpp>

#include <cstdint>
#include <unordered_map>
#include <vector>

#if defined(__APPLE__)
#import <Metal/Metal.h>
#endif

namespace flux {

struct GlyphKey {
  std::uint32_t fontId = 0;
  std::uint16_t glyphId = 0;
  std::uint16_t sizeQ8 = 0; // fontSize * 4, quantized to 0.25pt steps

  bool operator==(GlyphKey const& o) const noexcept {
    return fontId == o.fontId && glyphId == o.glyphId && sizeQ8 == o.sizeQ8;
  }
};

struct GlyphKeyHash {
  std::size_t operator()(GlyphKey const& k) const noexcept;
};

struct AtlasEntry {
  std::uint16_t u = 0;
  std::uint16_t v = 0;
  std::uint16_t width = 0;
  std::uint16_t height = 0;
  Point bearing{};
};

class GlyphAtlas {
public:
  GlyphAtlas(id<MTLDevice> device, TextSystem& textSystem);

  AtlasEntry const& getOrUpload(GlyphKey const& key);

  id<MTLTexture> texture() const { return texture_; }

  void grow();

  std::uint32_t atlasPixelWidth() const { return atlasWidth_; }
  std::uint32_t atlasPixelHeight() const { return atlasHeight_; }

private:
  AtlasEntry allocateAndUpload(GlyphKey const& key);

  id<MTLDevice> device_{nil};
  id<MTLTexture> texture_{nil};
  TextSystem& textSystem_;

  std::uint32_t atlasWidth_ = 1024;
  std::uint32_t atlasHeight_ = 1024;

  std::uint32_t shelfX_ = 1;
  std::uint32_t shelfY_ = 1;
  std::uint32_t shelfH_ = 0;

  std::unordered_map<GlyphKey, AtlasEntry, GlyphKeyHash> entries_;
};

} // namespace flux
