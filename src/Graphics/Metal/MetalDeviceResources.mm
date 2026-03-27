#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>
#import <simd/simd.h>

#include "Graphics/Metal/MetalDeviceResources.hpp"
#include "Graphics/Metal/MetalShaderLibrary.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace flux {

namespace {

constexpr NSUInteger kQuadStripCount = 4;

std::uint32_t blendModeKey(BlendMode m) { return static_cast<std::uint32_t>(static_cast<int>(m)); }

void setSrcOverBlend(MTLRenderPipelineColorAttachmentDescriptor* att) {
  att.blendingEnabled = YES;
  att.rgbBlendOperation = MTLBlendOperationAdd;
  att.alphaBlendOperation = MTLBlendOperationAdd;
  att.sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
  att.destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
  att.sourceAlphaBlendFactor = MTLBlendFactorOne;
  att.destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
}

void applyBlendModeToAttachment(MTLRenderPipelineColorAttachmentDescriptor* att, BlendMode mode) {
  switch (mode) {
  case BlendMode::Normal:
  case BlendMode::SrcOver:
    setSrcOverBlend(att);
    return;
  case BlendMode::Multiply:
    att.blendingEnabled = YES;
    att.rgbBlendOperation = MTLBlendOperationAdd;
    att.alphaBlendOperation = MTLBlendOperationAdd;
    att.sourceRGBBlendFactor = MTLBlendFactorDestinationColor;
    att.destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    att.sourceAlphaBlendFactor = MTLBlendFactorDestinationAlpha;
    att.destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    return;
  case BlendMode::Screen:
    att.blendingEnabled = YES;
    att.rgbBlendOperation = MTLBlendOperationAdd;
    att.alphaBlendOperation = MTLBlendOperationAdd;
    att.sourceRGBBlendFactor = MTLBlendFactorOneMinusDestinationColor;
    att.destinationRGBBlendFactor = MTLBlendFactorOne;
    att.sourceAlphaBlendFactor = MTLBlendFactorOne;
    att.destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    return;
  case BlendMode::Darken:
    att.blendingEnabled = YES;
    att.rgbBlendOperation = MTLBlendOperationMin;
    att.alphaBlendOperation = MTLBlendOperationAdd;
    att.sourceRGBBlendFactor = MTLBlendFactorOne;
    att.destinationRGBBlendFactor = MTLBlendFactorOne;
    att.sourceAlphaBlendFactor = MTLBlendFactorOne;
    att.destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    return;
  case BlendMode::Lighten:
    att.blendingEnabled = YES;
    att.rgbBlendOperation = MTLBlendOperationMax;
    att.alphaBlendOperation = MTLBlendOperationAdd;
    att.sourceRGBBlendFactor = MTLBlendFactorOne;
    att.destinationRGBBlendFactor = MTLBlendFactorOne;
    att.sourceAlphaBlendFactor = MTLBlendFactorOne;
    att.destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    return;
  case BlendMode::Clear:
    att.blendingEnabled = YES;
    att.rgbBlendOperation = MTLBlendOperationAdd;
    att.alphaBlendOperation = MTLBlendOperationAdd;
    att.sourceRGBBlendFactor = MTLBlendFactorZero;
    att.destinationRGBBlendFactor = MTLBlendFactorZero;
    att.sourceAlphaBlendFactor = MTLBlendFactorZero;
    att.destinationAlphaBlendFactor = MTLBlendFactorZero;
    return;
  case BlendMode::Src:
    att.blendingEnabled = YES;
    att.rgbBlendOperation = MTLBlendOperationAdd;
    att.alphaBlendOperation = MTLBlendOperationAdd;
    att.sourceRGBBlendFactor = MTLBlendFactorOne;
    att.destinationRGBBlendFactor = MTLBlendFactorZero;
    att.sourceAlphaBlendFactor = MTLBlendFactorOne;
    att.destinationAlphaBlendFactor = MTLBlendFactorZero;
    return;
  case BlendMode::Dst:
    att.blendingEnabled = YES;
    att.rgbBlendOperation = MTLBlendOperationAdd;
    att.alphaBlendOperation = MTLBlendOperationAdd;
    att.sourceRGBBlendFactor = MTLBlendFactorZero;
    att.destinationRGBBlendFactor = MTLBlendFactorOne;
    att.sourceAlphaBlendFactor = MTLBlendFactorZero;
    att.destinationAlphaBlendFactor = MTLBlendFactorOne;
    return;
  case BlendMode::DstOver:
    att.blendingEnabled = YES;
    att.rgbBlendOperation = MTLBlendOperationAdd;
    att.alphaBlendOperation = MTLBlendOperationAdd;
    att.sourceRGBBlendFactor = MTLBlendFactorOneMinusDestinationAlpha;
    att.destinationRGBBlendFactor = MTLBlendFactorOne;
    att.sourceAlphaBlendFactor = MTLBlendFactorOneMinusDestinationAlpha;
    att.destinationAlphaBlendFactor = MTLBlendFactorOne;
    return;
  case BlendMode::SrcIn:
    att.blendingEnabled = YES;
    att.rgbBlendOperation = MTLBlendOperationAdd;
    att.alphaBlendOperation = MTLBlendOperationAdd;
    att.sourceRGBBlendFactor = MTLBlendFactorDestinationAlpha;
    att.destinationRGBBlendFactor = MTLBlendFactorZero;
    att.sourceAlphaBlendFactor = MTLBlendFactorDestinationAlpha;
    att.destinationAlphaBlendFactor = MTLBlendFactorZero;
    return;
  case BlendMode::DstIn:
    att.blendingEnabled = YES;
    att.rgbBlendOperation = MTLBlendOperationAdd;
    att.alphaBlendOperation = MTLBlendOperationAdd;
    att.sourceRGBBlendFactor = MTLBlendFactorZero;
    att.destinationRGBBlendFactor = MTLBlendFactorSourceAlpha;
    att.sourceAlphaBlendFactor = MTLBlendFactorZero;
    att.destinationAlphaBlendFactor = MTLBlendFactorSourceAlpha;
    return;
  case BlendMode::SrcOut:
    att.blendingEnabled = YES;
    att.rgbBlendOperation = MTLBlendOperationAdd;
    att.alphaBlendOperation = MTLBlendOperationAdd;
    att.sourceRGBBlendFactor = MTLBlendFactorOneMinusDestinationAlpha;
    att.destinationRGBBlendFactor = MTLBlendFactorZero;
    att.sourceAlphaBlendFactor = MTLBlendFactorOneMinusDestinationAlpha;
    att.destinationAlphaBlendFactor = MTLBlendFactorZero;
    return;
  case BlendMode::DstOut:
    att.blendingEnabled = YES;
    att.rgbBlendOperation = MTLBlendOperationAdd;
    att.alphaBlendOperation = MTLBlendOperationAdd;
    att.sourceRGBBlendFactor = MTLBlendFactorZero;
    att.destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    att.sourceAlphaBlendFactor = MTLBlendFactorZero;
    att.destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    return;
  default:
    // Overlay / soft-light / non-separable / Xor / SrcAtop / DstAtop: approximate as Normal until shader blend.
    setSrcOverBlend(att);
    return;
  }
}

id<MTLRenderPipelineState> makePipeline(id<MTLDevice> device, id<MTLLibrary> lib, NSString* vert, NSString* frag,
                                        MTLPixelFormat colorFormat, BlendMode blendMode) {
  id<MTLFunction> vf = [lib newFunctionWithName:vert];
  id<MTLFunction> ff = [lib newFunctionWithName:frag];
  if (!vf || !ff) {
    return nil;
  }
  MTLRenderPipelineDescriptor* d = [[MTLRenderPipelineDescriptor alloc] init];
  d.vertexFunction = vf;
  d.fragmentFunction = ff;
  d.colorAttachments[0].pixelFormat = colorFormat;
  applyBlendModeToAttachment(d.colorAttachments[0], blendMode);
  NSError* err = nil;
  id<MTLRenderPipelineState> pso = [device newRenderPipelineStateWithDescriptor:d error:&err];
  if (!pso && err) {
    NSLog(@"Flux MetalDeviceResources: pipeline error: %@", err);
  }
  return pso;
}

id<MTLRenderPipelineState> makePathPipeline(id<MTLDevice> device, id<MTLLibrary> lib, MTLPixelFormat colorFormat,
                                            BlendMode blendMode) {
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
  applyBlendModeToAttachment(d.colorAttachments[0], blendMode);
  NSError* err = nil;
  id<MTLRenderPipelineState> pso = [device newRenderPipelineStateWithDescriptor:d error:&err];
  if (!pso && err) {
    NSLog(@"Flux MetalDeviceResources: path pipeline error: %@", err);
  }
  return pso;
}

} // namespace

id<MTLRenderPipelineState> MetalDeviceResources::rectPSO(BlendMode mode) {
  const std::uint32_t k = blendModeKey(mode);
  if (auto it = rectPSOCache_.find(k); it != rectPSOCache_.end()) {
    return it->second;
  }
  id<MTLRenderPipelineState> pso =
      makePipeline(device_, lib_, @"rect_sdf_vert", @"rect_sdf_frag", layer_.pixelFormat, mode);
  if (!pso) {
    throw std::runtime_error("MetalDeviceResources: rect pipeline creation failed");
  }
  rectPSOCache_[k] = pso;
  return pso;
}

id<MTLRenderPipelineState> MetalDeviceResources::linePSO(BlendMode mode) {
  const std::uint32_t k = blendModeKey(mode);
  if (auto it = linePSOCache_.find(k); it != linePSOCache_.end()) {
    return it->second;
  }
  id<MTLRenderPipelineState> pso =
      makePipeline(device_, lib_, @"line_sdf_vert", @"line_sdf_frag", layer_.pixelFormat, mode);
  if (!pso) {
    throw std::runtime_error("MetalDeviceResources: line pipeline creation failed");
  }
  linePSOCache_[k] = pso;
  return pso;
}

id<MTLRenderPipelineState> MetalDeviceResources::pathPSO(BlendMode mode) {
  const std::uint32_t k = blendModeKey(mode);
  if (auto it = pathPSOCache_.find(k); it != pathPSOCache_.end()) {
    return it->second;
  }
  id<MTLRenderPipelineState> pso = makePathPipeline(device_, lib_, layer_.pixelFormat, mode);
  if (!pso) {
    throw std::runtime_error("MetalDeviceResources: path pipeline creation failed");
  }
  pathPSOCache_[k] = pso;
  return pso;
}

MetalDeviceResources::MetalDeviceResources(CAMetalLayer* layer) : layer_(layer) {
  device_ = layer_.device ? layer_.device : MTLCreateSystemDefaultDevice();
  layer_.device = device_;
  layer_.pixelFormat = MTLPixelFormatBGRA8Unorm;
  queue_ = [device_ newCommandQueue];

  lib_ = flux::detail::fluxLoadShaderLibrary(device_);

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
