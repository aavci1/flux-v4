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

  MetalFrameRecorder() = default;
  ~MetalFrameRecorder();
  MetalFrameRecorder(MetalFrameRecorder const&) = delete;
  MetalFrameRecorder& operator=(MetalFrameRecorder const&) = delete;
  MetalFrameRecorder(MetalFrameRecorder&& other) noexcept;
  MetalFrameRecorder& operator=(MetalFrameRecorder&& other) noexcept;

  void clear();
};

} // namespace flux
