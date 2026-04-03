#pragma once

#include <Flux/Graphics/Styles.hpp>

#include <cstdint>
#include <type_traits>
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
  /// Premultiplied shadow tint; .w is alpha scale.
  vector_float4 shadowColor;
  /// .xy = offset (device px), .z = blur radius (device px, uniform scale), .w unused.
  vector_float4 shadowGeom;
};

static_assert(std::is_trivially_copyable_v<MetalRectInstance>);

/// Per-vertex glyph payload (two triangles per glyph). Matches `GlyphVertexIn` in `CanvasShaders.metal`.
struct MetalGlyphVertex {
  vector_float2 pos{};
  vector_float2 uv{};
  vector_float4 color{};
};

/// GPU instance for `image_sdf_vert` / `image_sdf_frag` (matches `ImageInstance` in `CanvasShaders.metal`).
struct MetalImageInstance {
  MetalRectInstance sdf;
  vector_float4 uvBounds; // u0, v0, u1, v1 in normalized texture coordinates
  vector_float2 texSizeInv;
  vector_float2 imageModePad; // x: 0 = clamp UV bounds, 1 = tile (repeat sampler)
};

static_assert(std::is_trivially_copyable_v<MetalImageInstance>);

struct MetalDrawOp {
  enum Kind : std::uint8_t { Rect, Line, PathMesh, GlyphMesh, Image } kind = Rect;
  union {
    MetalRectInstance rectInst;
    MetalImageInstance imageInst;
  };
  std::uint32_t pathStart = 0;
  std::uint32_t pathCount = 0;
  std::uint32_t glyphStart = 0;
  std::uint32_t glyphVertexCount = 0;
  /// Blend state for this draw (fixed-function blend in the PSO).
  BlendMode blendMode = BlendMode::Normal;
  /// Valid when `kind == Image` — `id<MTLTexture>` retained per op (`__bridge_retained`); released in `MetalFrameRecorder::clear`.
  void* texture = nullptr;
  bool repeatSampler = false;
  /// GPU scissor for this draw (physical pixels). If `false`, use full drawable in `present()`.
  bool scissorValid = false;
  std::uint32_t scissorX = 0;
  std::uint32_t scissorY = 0;
  std::uint32_t scissorW = 0;
  std::uint32_t scissorH = 0;
};

} // namespace flux
