#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>
#import <simd/simd.h>

#include "Graphics/Metal/MetalDeviceResources.hpp"
#include "Graphics/Metal/MetalShaderLibrary.hpp"

#include <algorithm>
#include <cstring>

namespace flux {

namespace {

constexpr NSUInteger kQuadStripCount = 4;

id<MTLRenderPipelineState> makePipeline(id<MTLDevice> device, id<MTLLibrary> lib, NSString* vert, NSString* frag,
                                        MTLPixelFormat colorFormat, bool blend) {
  id<MTLFunction> vf = [lib newFunctionWithName:vert];
  id<MTLFunction> ff = [lib newFunctionWithName:frag];
  if (!vf || !ff) {
    return nil;
  }
  MTLRenderPipelineDescriptor* d = [[MTLRenderPipelineDescriptor alloc] init];
  d.vertexFunction = vf;
  d.fragmentFunction = ff;
  d.colorAttachments[0].pixelFormat = colorFormat;
  if (blend) {
    d.colorAttachments[0].blendingEnabled = YES;
    d.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    d.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    d.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
    d.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
  }
  NSError* err = nil;
  id<MTLRenderPipelineState> pso = [device newRenderPipelineStateWithDescriptor:d error:&err];
  if (!pso && err) {
    NSLog(@"Flux MetalDeviceResources: pipeline error: %@", err);
  }
  return pso;
}

id<MTLRenderPipelineState> makePathPipeline(id<MTLDevice> device, id<MTLLibrary> lib, MTLPixelFormat colorFormat,
                                              bool blend) {
  id<MTLFunction> vf = [lib newFunctionWithName:@"path_vert"];
  id<MTLFunction> ff = [lib newFunctionWithName:@"path_frag"];
  if (!vf || !ff) {
    return nil;
  }
  MTLVertexDescriptor* vd = [[MTLVertexDescriptor alloc] init];
  vd.attributes[0].format = MTLVertexFormatFloat2;
  vd.attributes[0].offset = 0;
  vd.attributes[0].bufferIndex = 0;
  vd.attributes[1].format = MTLVertexFormatFloat4;
  vd.attributes[1].offset = 8;
  vd.attributes[1].bufferIndex = 0;
  vd.attributes[2].format = MTLVertexFormatFloat2;
  vd.attributes[2].offset = 24;
  vd.attributes[2].bufferIndex = 0;
  vd.layouts[0].stride = 32;
  vd.layouts[0].stepRate = 1;
  vd.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;

  MTLRenderPipelineDescriptor* d = [[MTLRenderPipelineDescriptor alloc] init];
  d.vertexFunction = vf;
  d.fragmentFunction = ff;
  d.vertexDescriptor = vd;
  d.colorAttachments[0].pixelFormat = colorFormat;
  if (blend) {
    d.colorAttachments[0].blendingEnabled = YES;
    d.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    d.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    d.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
    d.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
  }
  NSError* err = nil;
  id<MTLRenderPipelineState> pso = [device newRenderPipelineStateWithDescriptor:d error:&err];
  if (!pso && err) {
    NSLog(@"Flux MetalDeviceResources: path pipeline error: %@", err);
  }
  return pso;
}

} // namespace

MetalDeviceResources::MetalDeviceResources(CAMetalLayer* layer) : layer_(layer) {
  device_ = layer_.device ? layer_.device : MTLCreateSystemDefaultDevice();
  layer_.device = device_;
  layer_.pixelFormat = MTLPixelFormatBGRA8Unorm;
  queue_ = [device_ newCommandQueue];

  lib_ = flux::detail::fluxLoadShaderLibrary(device_);

  MTLPixelFormat pf = layer_.pixelFormat;
  rectPSO_ = makePipeline(device_, lib_, @"rect_sdf_vert", @"rect_sdf_frag", pf, true);
  linePSO_ = makePipeline(device_, lib_, @"line_sdf_vert", @"line_sdf_frag", pf, true);
  pathPSO_ = makePathPipeline(device_, lib_, pf, true);
  if (!rectPSO_ || !linePSO_ || !pathPSO_) {
    throw std::runtime_error("MetalDeviceResources: pipeline creation failed");
  }

  static const vector_float2 kQuadStrip[kQuadStripCount] = {
      {-1.f, -1.f},
      {1.f, -1.f},
      {-1.f, 1.f},
      {1.f, 1.f},
  };
  quadBuffer_ = [device_ newBufferWithBytes:kQuadStrip length:sizeof(kQuadStrip) options:MTLResourceStorageModeShared];
}

MetalDeviceResources::~MetalDeviceResources() = default;

void MetalDeviceResources::ensureInstanceArenaCapacity(std::uint32_t instanceCount) {
  if (instanceCount == 0) {
    return;
  }
  if (instanceCount <= instanceArenaCapacityInstanceCount_) {
    return;
  }
  const std::uint32_t newCap = instanceArenaCapacityInstanceCount_ == 0
                                   ? instanceCount
                                   : std::max(instanceCount, instanceArenaCapacityInstanceCount_ * 2);
  instanceArena_ = [device_ newBufferWithLength:newCap * sizeof(MetalRectInstance) options:MTLResourceStorageModeShared];
  instanceArenaCapacityInstanceCount_ = newCap;
}

void MetalDeviceResources::ensurePathVertexArenaCapacity(std::uint32_t byteCount) {
  if (byteCount == 0) {
    return;
  }
  if (byteCount <= pathVertexArenaCapacityBytes_) {
    return;
  }
  const std::uint32_t newCap =
      pathVertexArenaCapacityBytes_ == 0 ? byteCount : std::max(byteCount, pathVertexArenaCapacityBytes_ * 2);
  pathVertexArena_ = [device_ newBufferWithLength:newCap options:MTLResourceStorageModeShared];
  pathVertexArenaCapacityBytes_ = newCap;
}

void MetalDeviceResources::uploadInstanceInstances(const std::vector<MetalDrawOp>& ops) {
  NSUInteger rectLineInstanceCount = 0;
  for (const MetalDrawOp& op : ops) {
    if (op.kind == MetalDrawOp::Rect || op.kind == MetalDrawOp::Line) {
      ++rectLineInstanceCount;
    }
  }
  ensureInstanceArenaCapacity(static_cast<std::uint32_t>(rectLineInstanceCount));
  if (rectLineInstanceCount == 0) {
    return;
  }
  auto* dst = static_cast<MetalRectInstance*>([instanceArena_ contents]);
  NSUInteger slot = 0;
  for (const MetalDrawOp& op : ops) {
    if (op.kind == MetalDrawOp::Rect || op.kind == MetalDrawOp::Line) {
      dst[slot++] = op.rectInst;
    }
  }
}

void MetalDeviceResources::uploadPathVertices(const std::vector<PathVertex>& pathVerts) {
  const NSUInteger pathVertBytes = static_cast<NSUInteger>(pathVerts.size() * sizeof(PathVertex));
  ensurePathVertexArenaCapacity(static_cast<std::uint32_t>(pathVertBytes));
  if (pathVertBytes == 0) {
    return;
  }
  std::memcpy([pathVertexArena_ contents], pathVerts.data(), pathVertBytes);
}

} // namespace flux
