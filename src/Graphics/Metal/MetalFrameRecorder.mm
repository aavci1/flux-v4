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
      glyphVerts(std::move(other.glyphVerts)) {
  other.rectOps.clear();
  other.imageOps.clear();
  other.pathOps.clear();
  other.glyphOps.clear();
  other.opOrder.clear();
  other.pathVerts.clear();
  other.glyphVerts.clear();
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
  other.rectOps.clear();
  other.imageOps.clear();
  other.pathOps.clear();
  other.glyphOps.clear();
  other.opOrder.clear();
  other.pathVerts.clear();
  other.glyphVerts.clear();
  return *this;
}

void MetalFrameRecorder::clear() {
  for (auto& op : imageOps) {
    if (op.texture) {
      (void)(__bridge_transfer id<MTLTexture>)op.texture;
    }
    op.texture = nullptr;
  }
  rectOps.clear();
  imageOps.clear();
  pathOps.clear();
  glyphOps.clear();
  opOrder.clear();
  pathVerts.clear();
  glyphVerts.clear();
}

} // namespace flux
