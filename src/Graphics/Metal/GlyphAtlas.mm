#import <Metal/Metal.h>

#include "Graphics/Metal/GlyphAtlas.hpp"

#include <algorithm>
#include <stdexcept>
#include <vector>

namespace flux {

std::size_t GlyphKeyHash::operator()(GlyphKey const& k) const noexcept {
  std::size_t h = std::hash<std::uint32_t>{}(k.fontId);
  h ^= std::hash<std::uint32_t>{}((static_cast<std::uint32_t>(k.glyphId) << 16) | k.sizeQ8) + 0x9e3779b9 +
       (h << 6) + (h >> 2);
  return h;
}

namespace {

constexpr std::uint32_t kMaxAtlasDim = 4096;
constexpr std::uint32_t kCellPad = 1; // 1px border inside cell around glyph bitmap

void uploadR8(id<MTLTexture> tex, std::uint32_t x, std::uint32_t y, std::uint32_t w, std::uint32_t h,
              std::vector<std::uint8_t> const& r8) {
  if (w == 0 || h == 0) {
    return;
  }
  MTLRegion region = {{x, y, 0}, {w, h, 1}};
  [tex replaceRegion:region mipmapLevel:0 withBytes:r8.data() bytesPerRow:w];
}

} // namespace

GlyphAtlas::GlyphAtlas(id<MTLDevice> device, TextSystem& textSystem)
    : device_(device), textSystem_(textSystem) {
  MTLTextureDescriptor* d = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Unorm
                                                                                 width:atlasWidth_
                                                                                height:atlasHeight_
                                                                             mipmapped:NO];
  d.usage = MTLTextureUsageShaderRead;
  d.storageMode = MTLStorageModeShared;
  texture_ = [device_ newTextureWithDescriptor:d];
  if (!texture_) {
    throw std::runtime_error("GlyphAtlas: failed to create atlas texture");
  }
}

void GlyphAtlas::grow() {
  if (atlasWidth_ >= kMaxAtlasDim || atlasHeight_ >= kMaxAtlasDim) {
    throw std::runtime_error("GlyphAtlas: atlas exceeds maximum size");
  }

  std::vector<GlyphKey> keys;
  keys.reserve(entries_.size());
  for (auto const& e : entries_) {
    keys.push_back(e.first);
  }
  entries_.clear();
  shelfX_ = 1;
  shelfY_ = 1;
  shelfH_ = 0;

  atlasWidth_ = std::min(atlasWidth_ * 2, kMaxAtlasDim);
  atlasHeight_ = std::min(atlasHeight_ * 2, kMaxAtlasDim);

  MTLTextureDescriptor* d = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Unorm
                                                                                 width:atlasWidth_
                                                                                height:atlasHeight_
                                                                             mipmapped:NO];
  d.usage = MTLTextureUsageShaderRead;
  d.storageMode = MTLStorageModeShared;
  id<MTLTexture> newTex = [device_ newTextureWithDescriptor:d];
  if (!newTex) {
    throw std::runtime_error("GlyphAtlas: grow failed to allocate texture");
  }
  texture_ = newTex;

  for (GlyphKey const& k : keys) {
    (void)getOrUpload(k);
  }
}

AtlasEntry GlyphAtlas::allocateAndUpload(GlyphKey const& key) {
  float const size = static_cast<float>(key.sizeQ8) / 4.f;
  std::uint32_t gw = 0;
  std::uint32_t gh = 0;
  Point bearing{};
  std::vector<std::uint8_t> bits =
      textSystem_.rasterizeGlyph(key.fontId, key.glyphId, size, gw, gh, bearing);
  if (gw == 0 || gh == 0 || bits.empty()) {
    return AtlasEntry{};
  }

  std::uint32_t const cellW = gw + kCellPad * 2;
  std::uint32_t const cellH = gh + kCellPad * 2;

  auto ensureSpace = [&] {
    if (shelfX_ + cellW + 1 > atlasWidth_) {
      shelfY_ += shelfH_ + 1;
      shelfX_ = 1;
      shelfH_ = 0;
    }
    return shelfY_ + cellH + 1 <= atlasHeight_;
  };

  while (!ensureSpace()) {
    grow();
  }

  shelfH_ = std::max(shelfH_, cellH);

  std::uint32_t const u = shelfX_ + kCellPad;
  std::uint32_t const v = shelfY_ + kCellPad;
  uploadR8(texture_, u, v, gw, gh, bits);

  shelfX_ += cellW + 1;

  AtlasEntry e{};
  e.u = static_cast<std::uint16_t>(u);
  e.v = static_cast<std::uint16_t>(v);
  e.width = static_cast<std::uint16_t>(gw);
  e.height = static_cast<std::uint16_t>(gh);
  e.bearing = bearing;
  return e;
}

AtlasEntry const& GlyphAtlas::getOrUpload(GlyphKey const& key) {
  auto it = entries_.find(key);
  if (it != entries_.end()) {
    return it->second;
  }
  AtlasEntry e = allocateAndUpload(key);
  auto ins = entries_.emplace(key, e);
  return ins.first->second;
}

} // namespace flux
