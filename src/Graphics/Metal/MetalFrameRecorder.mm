#import <Metal/Metal.h>

#include "Graphics/Metal/MetalFrameRecorder.hpp"

namespace flux {

MetalFrameRecorder::~MetalFrameRecorder() {
  clear();
}

MetalFrameRecorder::MetalFrameRecorder(MetalFrameRecorder&& other) noexcept
    : ops(std::move(other.ops)),
      pathVerts(std::move(other.pathVerts)),
      glyphVerts(std::move(other.glyphVerts)) {
  other.ops.clear();
  other.pathVerts.clear();
  other.glyphVerts.clear();
}

MetalFrameRecorder& MetalFrameRecorder::operator=(MetalFrameRecorder&& other) noexcept {
  if (this == &other) {
    return *this;
  }
  clear();
  ops = std::move(other.ops);
  pathVerts = std::move(other.pathVerts);
  glyphVerts = std::move(other.glyphVerts);
  other.ops.clear();
  other.pathVerts.clear();
  other.glyphVerts.clear();
  return *this;
}

void MetalFrameRecorder::clear() {
  for (auto& op : ops) {
    if (op.kind == MetalDrawOp::Image && op.texture) {
      (void)(__bridge_transfer id<MTLTexture>)op.texture;
      op.texture = nullptr;
    }
  }
  ops.clear();
  pathVerts.clear();
  glyphVerts.clear();
}

} // namespace flux
