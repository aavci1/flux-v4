#import <Metal/Metal.h>

#include "Graphics/Metal/MetalFrameRecorder.hpp"

namespace flux {

MetalFrameRecorder::~MetalFrameRecorder() {
  clear();
}

MetalFrameRecorder::MetalFrameRecorder(MetalFrameRecorder&& other) noexcept
    : rectOps(std::move(other.rectOps)),
      imageOps(std::move(other.imageOps)),
      pathOps(std::move(other.pathOps)),
      glyphOps(std::move(other.glyphOps)),
      opOrder(std::move(other.opOrder)),
      pathVerts(std::move(other.pathVerts)),
      glyphVerts(std::move(other.glyphVerts)),
      glyphVertexSources(std::move(other.glyphVertexSources)),
      glyphVertexCount(other.glyphVertexCount),
      preparedGlyphVertexBuffer(other.preparedGlyphVertexBuffer),
      preparedGlyphVertexCapacity(other.preparedGlyphVertexCapacity) {
  other.rectOps.clear();
  other.imageOps.clear();
  other.pathOps.clear();
  other.glyphOps.clear();
  other.opOrder.clear();
  other.pathVerts.clear();
  other.glyphVerts.clear();
  other.glyphVertexSources.clear();
  other.glyphVertexCount = 0;
  other.preparedGlyphVertexBuffer = nullptr;
  other.preparedGlyphVertexCapacity = 0;
}

MetalFrameRecorder& MetalFrameRecorder::operator=(MetalFrameRecorder&& other) noexcept {
  if (this == &other) {
    return *this;
  }
  clear();
  rectOps = std::move(other.rectOps);
  imageOps = std::move(other.imageOps);
  pathOps = std::move(other.pathOps);
  glyphOps = std::move(other.glyphOps);
  opOrder = std::move(other.opOrder);
  pathVerts = std::move(other.pathVerts);
  glyphVerts = std::move(other.glyphVerts);
  glyphVertexSources = std::move(other.glyphVertexSources);
  glyphVertexCount = other.glyphVertexCount;
  preparedGlyphVertexBuffer = other.preparedGlyphVertexBuffer;
  preparedGlyphVertexCapacity = other.preparedGlyphVertexCapacity;
  other.rectOps.clear();
  other.imageOps.clear();
  other.pathOps.clear();
  other.glyphOps.clear();
  other.opOrder.clear();
  other.pathVerts.clear();
  other.glyphVerts.clear();
  other.glyphVertexSources.clear();
  other.glyphVertexCount = 0;
  other.preparedGlyphVertexBuffer = nullptr;
  other.preparedGlyphVertexCapacity = 0;
  return *this;
}

void MetalFrameRecorder::clear() {
  for (auto& op : imageOps) {
    if (op.texture) {
      (void)(__bridge_transfer id<MTLTexture>)op.texture;
    }
    op.texture = nullptr;
  }
  if (preparedGlyphVertexBuffer) {
    (void)(__bridge_transfer id<MTLBuffer>)preparedGlyphVertexBuffer;
    preparedGlyphVertexBuffer = nullptr;
    preparedGlyphVertexCapacity = 0;
  }
  rectOps.clear();
  imageOps.clear();
  pathOps.clear();
  glyphOps.clear();
  opOrder.clear();
  pathVerts.clear();
  glyphVerts.clear();
  glyphVertexSources.clear();
  glyphVertexCount = 0;
}

} // namespace flux
