#pragma once

#include <Flux/Graphics/Styles.hpp>

#include <cstdint>
#include <vector>

#include <simd/simd.h>

namespace flux {

/// GPU instance payload for rect/line SDF shaders (matches `RectInstance` in `CanvasShaders.metal`).
struct MetalRectInstance {
  vector_float4 rect;
  vector_float4 corners;
  vector_float4 fillColor;
  vector_float4 strokeColor;
  vector_float2 strokeWidthOpacity;
  vector_float2 viewport;
  vector_float4 rotationPad;
};

struct MetalDrawOp {
  enum Kind : std::uint8_t { Rect, Line, PathMesh } kind = Rect;
  MetalRectInstance rectInst{};
  std::uint32_t pathStart = 0;
  std::uint32_t pathCount = 0;
  /// Blend state for this draw (fixed-function blend in the PSO).
  BlendMode blendMode = BlendMode::Normal;
};

} // namespace flux
