#pragma once

#include "Graphics/Metal/MetalCanvasTypes.hpp"
#include "Graphics/PathFlattener.hpp"

#include <cstdint>
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
  id<MTLRenderPipelineState> rectPSO() const { return rectPSO_; }
  id<MTLRenderPipelineState> linePSO() const { return linePSO_; }
  id<MTLRenderPipelineState> pathPSO() const { return pathPSO_; }
  id<MTLBuffer> quadBuffer() const { return quadBuffer_; }

  /// Pack rect/line instance data for this frame (submission order preserved).
  void uploadInstanceInstances(const std::vector<MetalDrawOp>& ops);
  /// Copy path vertices into the path arena (no-op if empty).
  void uploadPathVertices(const std::vector<PathVertex>& pathVerts);

  id<MTLBuffer> instanceArenaBuffer() const { return instanceArena_; }
  id<MTLBuffer> pathVertexArenaBuffer() const { return pathVertexArena_; }

private:
  CAMetalLayer* layer_{nil};
  id<MTLDevice> device_{nil};
  id<MTLCommandQueue> queue_{nil};
  id<MTLLibrary> lib_{nil};
  id<MTLRenderPipelineState> rectPSO_{nil};
  id<MTLRenderPipelineState> linePSO_{nil};
  id<MTLRenderPipelineState> pathPSO_{nil};
  id<MTLBuffer> quadBuffer_{nil};

  id<MTLBuffer> instanceArena_{nil};
  std::uint32_t instanceArenaCapacityInstanceCount_{0};
  id<MTLBuffer> pathVertexArena_{nil};
  std::uint32_t pathVertexArenaCapacityBytes_{0};

  void ensureInstanceArenaCapacity(std::uint32_t instanceCount);
  void ensurePathVertexArenaCapacity(std::uint32_t byteCount);
};

} // namespace flux
