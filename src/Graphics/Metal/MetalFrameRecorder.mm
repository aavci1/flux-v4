#import <Metal/Metal.h>

#include "Graphics/Metal/MetalFrameRecorder.hpp"

namespace flux {

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
