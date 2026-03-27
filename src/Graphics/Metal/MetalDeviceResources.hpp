#pragma once

#include <Flux/Graphics/Styles.hpp>

#include "Graphics/Metal/MetalCanvasTypes.hpp"
#include "Graphics/PathFlattener.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>

#if defined(__APPLE__)
#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>
#endif

namespace flux {

/**
 * Metal device, pipelines, static geometry, and per-frame GPU arenas for instance/path data.
 * Owned by `MetalCanvas`; keeps buffer pools co-located with the PSOs that consume them.
 */
class MetalDeviceResources {
public:
  explicit MetalDeviceResources(CAMetalLayer* layer);
  ~MetalDeviceResources();

  MetalDeviceResources(const MetalDeviceResources&) = delete;
  MetalDeviceResources& operator=(const MetalDeviceResources&) = delete;
  MetalDeviceResources(MetalDeviceResources&&) = delete;
  MetalDeviceResources& operator=(MetalDeviceResources&&) = delete;

  CAMetalLayer* layer() const { return layer_; }
  id<MTLDevice> device() const { return device_; }
  id<MTLCommandQueue> queue() const { return queue_; }
  id<MTLRenderPipelineState> rectPSO(BlendMode mode);
  id<MTLRenderPipelineState> linePSO(BlendMode mode);
  id<MTLRenderPipelineState> pathPSO(BlendMode mode);
  id<MTLRenderPipelineState> glyphPSO(BlendMode mode);
  id<MTLRenderPipelineState> imagePSO(BlendMode mode);
  id<MTLSamplerState> linearSampler() const { return linearSampler_; }
  id<MTLSamplerState> repeatSampler() const { return repeatSampler_; }
  id<MTLBuffer> quadBuffer() const { return quadBuffer_; }

  /// Pack rect/line instance data for this frame (submission order preserved).
  void uploadInstanceInstances(const std::vector<MetalDrawOp>& ops);
  void uploadImageInstances(const std::vector<MetalDrawOp>& ops);
  /// Copy path vertices into the path arena (no-op if empty).
  void uploadPathVertices(const std::vector<PathVertex>& pathVerts);

  void uploadGlyphVertices(const std::vector<MetalGlyphVertex>& verts);

  id<MTLBuffer> instanceArenaBuffer() const { return instanceArena_; }
  id<MTLBuffer> imageInstanceArenaBuffer() const { return imageInstanceArena_; }
  id<MTLBuffer> pathVertexArenaBuffer() const { return pathVertexArena_; }
  id<MTLBuffer> glyphVertexArenaBuffer() const { return glyphVertexArena_; }

private:
  CAMetalLayer* layer_{nil};
  id<MTLDevice> device_{nil};
  id<MTLCommandQueue> queue_{nil};
  id<MTLLibrary> lib_{nil};
  std::unordered_map<std::uint32_t, id<MTLRenderPipelineState>> rectPSOCache_;
  std::unordered_map<std::uint32_t, id<MTLRenderPipelineState>> linePSOCache_;
  std::unordered_map<std::uint32_t, id<MTLRenderPipelineState>> pathPSOCache_;
  std::unordered_map<std::uint32_t, id<MTLRenderPipelineState>> glyphPSOCache_;
  std::unordered_map<std::uint32_t, id<MTLRenderPipelineState>> imagePSOCache_;
  id<MTLBuffer> quadBuffer_{nil};
  id<MTLSamplerState> linearSampler_{nil};
  id<MTLSamplerState> repeatSampler_{nil};

  id<MTLBuffer> instanceArena_{nil};
  std::uint32_t instanceArenaCapacityInstanceCount_{0};
  id<MTLBuffer> imageInstanceArena_{nil};
  std::uint32_t imageInstanceArenaCapacity_{0};
  id<MTLBuffer> pathVertexArena_{nil};
  std::uint32_t pathVertexArenaCapacityBytes_{0};
  id<MTLBuffer> glyphVertexArena_{nil};
  std::uint32_t glyphVertexArenaCapacityBytes_{0};

  void ensureInstanceArenaCapacity(std::uint32_t instanceCount);
  void ensureImageInstanceArenaCapacity(std::uint32_t instanceCount);
  void ensurePathVertexArenaCapacity(std::uint32_t byteCount);
  void ensureGlyphVertexArenaCapacity(std::uint32_t byteCount);
};

} // namespace flux
