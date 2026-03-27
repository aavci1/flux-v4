#pragma once

#include "Graphics/Metal/MetalCanvasTypes.hpp"
#include "Graphics/PathFlattener.hpp"

#include <vector>

namespace flux {

/// Per-frame CPU-side display list: primitive ops + accumulated path mesh vertices.
struct MetalFrameRecorder {
  std::vector<MetalDrawOp> ops;
  std::vector<PathVertex> pathVerts;
  std::vector<MetalGlyphVertex> glyphVerts;

  void clear() {
    ops.clear();
    pathVerts.clear();
    glyphVerts.clear();
  }
};

} // namespace flux
