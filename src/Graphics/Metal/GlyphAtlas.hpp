#pragma once

#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/TextSystem.hpp>

#include <cstdint>
#include <functional>
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

  /// Tier A: grow before drawing when shelf usage is high so `grow()` rarely runs mid-frame (which would
  /// invalidate UVs already written to the frame). Call from `Canvas::beginFrame` after clearing frame data.
  /// For a hard guarantee under unbounded unique glyphs per frame, a copy-preserving grow (tier C) is needed.
  void prepareForFrameBegin();

  /// Tier B: grow after a presented frame when utilization is still high, giving headroom for the next frame.
  void afterPresent();

  /// Debug: assert if `grow()` runs while the Metal canvas still holds glyph verts for the current frame.
  void setBeforeGrowCallback(std::function<void()> cb) { beforeGrow_ = std::move(cb); }

  std::uint32_t atlasPixelWidth() const { return atlasWidth_; }
  std::uint32_t atlasPixelHeight() const { return atlasHeight_; }

private:
  AtlasEntry allocateAndUpload(GlyphKey const& key);

  bool pressureHighForHeadroom() const;

  id<MTLDevice> device_{nil};
  id<MTLTexture> texture_{nil};
  TextSystem& textSystem_;

  std::uint32_t atlasWidth_ = 1024;
  std::uint32_t atlasHeight_ = 1024;

  std::uint32_t shelfX_ = 1;
  std::uint32_t shelfY_ = 1;
  std::uint32_t shelfH_ = 0;

  std::unordered_map<GlyphKey, AtlasEntry, GlyphKeyHash> entries_;

  std::function<void()> beforeGrow_{};
};

} // namespace flux
