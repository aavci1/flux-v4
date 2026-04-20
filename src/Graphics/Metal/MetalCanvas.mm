#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>
#import <simd/simd.h>

#include <Flux/Core/Application.hpp>
#include <Flux/Graphics/Canvas.hpp>
#include <Flux/Graphics/Image.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/Graphics/TextLayout.hpp>

#include "Graphics/Metal/GlyphAtlas.hpp"
#include "Graphics/Metal/MetalImage.hpp"
#include "Graphics/Metal/MetalCanvasTypes.hpp"
#include "Graphics/Metal/MetalDeviceResources.hpp"
#include "Graphics/Metal/MetalFrameRecorder.hpp"
#include "Graphics/Metal/MetalPathRasterizer.hpp"

namespace flux {
class Window;
}

#include "Graphics/Metal/MetalCanvas.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

namespace flux {

namespace {

constexpr NSUInteger kQuadStripCount = 4;
constexpr NSUInteger kFramesInFlight = 3;

vector_float4 toSimd4(const Color& c) { return simd_make_float4(c.r, c.g, c.b, c.a); }

vector_float4 cornersToSimd(const CornerRadius& cr) {
  return simd_make_float4(cr.topLeft, cr.topRight, cr.bottomRight, cr.bottomLeft);
}

Rect boundsOfTransformedRect(Rect const& r, Mat3 const& m) {
  Point p0 = m.apply({r.x, r.y});
  Point p1 = m.apply({r.x + r.width, r.y});
  Point p2 = m.apply({r.x, r.y + r.height});
  Point p3 = m.apply({r.x + r.width, r.y + r.height});
  float minX = std::min({p0.x, p1.x, p2.x, p3.x});
  float minY = std::min({p0.y, p1.y, p2.y, p3.y});
  float maxX = std::max({p0.x, p1.x, p2.x, p3.x});
  float maxY = std::max({p0.y, p1.y, p2.y, p3.y});
  return Rect::sharp(minX, minY, maxX - minX, maxY - minY);
}

bool intersects(Rect const& a, Rect const& b) { return a.intersects(b); }

Rect intersectRects(Rect const& a, Rect const& b) {
  const float x0 = std::max(a.x, b.x);
  const float y0 = std::max(a.y, b.y);
  const float x1 = std::min(a.x + a.width, b.x + b.width);
  const float y1 = std::min(a.y + a.height, b.y + b.height);
  if (x1 <= x0 || y1 <= y0) {
    return Rect::sharp(0, 0, 0, 0);
  }
  return Rect::sharp(x0, y0, x1 - x0, y1 - y0);
}

/// When `vis` is the axis-aligned bbox of (roundRect ∩ clip), corners on cut edges must be sharp — the
/// SDF round-rect assumes `vis` is the full shape, not a truncated round-rect (see Path::rect).
CornerRadius cornerRadiiAfterAxisAlignedClip(Rect const& full, Rect const& vis, CornerRadius const& cr) {
  constexpr float eps = 1e-3f;
  CornerRadius out = cr;
  if (vis.x > full.x + eps) {
    out.topLeft = 0.f;
    out.bottomLeft = 0.f;
  }
  if (vis.x + vis.width < full.x + full.width - eps) {
    out.topRight = 0.f;
    out.bottomRight = 0.f;
  }
  if (vis.y > full.y + eps) {
    out.topLeft = 0.f;
    out.topRight = 0.f;
  }
  if (vis.y + vis.height < full.y + full.height - eps) {
    out.bottomLeft = 0.f;
    out.bottomRight = 0.f;
  }
  return out;
}

void clampRoundRectCornerRadii(float w, float h, CornerRadius& r) {
  if (w <= 0.f || h <= 0.f) {
    return;
  }
  const float maxR = std::min(w, h) * 0.5f;
  r.topLeft = std::min(r.topLeft, maxR);
  r.topRight = std::min(r.topRight, maxR);
  r.bottomRight = std::min(r.bottomRight, maxR);
  r.bottomLeft = std::min(r.bottomLeft, maxR);
  auto fixEdge = [](float& a, float& b, float len) {
    if (a + b > len && len > 0.f) {
      const float s = len / (a + b);
      a *= s;
      b *= s;
    }
  };
  fixEdge(r.topLeft, r.topRight, w);
  fixEdge(r.bottomLeft, r.bottomRight, w);
  fixEdge(r.topLeft, r.bottomLeft, h);
  fixEdge(r.topRight, r.bottomRight, h);
}

bool tryDecomposeRotationTranslation(Mat3 const& m, float* outAngle, float* outTx, float* outTy) {
  const float a = m.m[0], b = m.m[1], c = m.m[3], d = m.m[4];
  const float det = a * d - b * c;
  const float sx = std::hypot(a, b);
  const float sy = std::hypot(c, d);
  constexpr float eps = 0.06f;
  if (std::abs(std::abs(det) - 1.f) > 0.08f) {
    return false;
  }
  if (std::abs(sx - sy) > eps) {
    return false;
  }
  if (std::abs(sx - 1.f) > 0.1f) {
    return false;
  }
  if (std::abs(a * c + b * d) > 0.06f) {
    return false;
  }
  *outAngle = std::atan2(b, a);
  *outTx = m.m[6];
  *outTy = m.m[7];
  return true;
}

/** Stroke-only paths that flatten to polylines (Move/Line only). */
bool pathIsMoveLineOnlyStroke(Path const& path) {
  if (path.commandCount() < 2) {
    return false;
  }
  for (size_t i = 0; i < path.commandCount(); ++i) {
    Path::CommandView cv = path.command(i);
    switch (cv.type) {
      case Path::CommandType::MoveTo:
      case Path::CommandType::LineTo:
      case Path::CommandType::SetWinding:
        break;
      default:
        return false;
    }
  }
  return true;
}

void appendGlyphQuad(std::vector<MetalGlyphVertex>& out, Mat3 const& M, float dpiX, float dpiY, Point tlLogical,
                     float gw, float gh, float u0, float v0, float u1, float v1, vector_float4 premulRgba) {
  Point const c0 = tlLogical;
  Point const c1 = {tlLogical.x + gw, tlLogical.y};
  Point const c2 = {tlLogical.x, tlLogical.y + gh};
  Point const c3 = {tlLogical.x + gw, tlLogical.y + gh};
  Point p0 = M.apply(c0);
  Point p1 = M.apply(c1);
  Point p2 = M.apply(c2);
  Point p3 = M.apply(c3);
  p0.x *= dpiX;
  p0.y *= dpiY;
  p1.x *= dpiX;
  p1.y *= dpiY;
  p2.x *= dpiX;
  p2.y *= dpiY;
  p3.x *= dpiX;
  p3.y *= dpiY;
  vector_float2 const uv0 = simd_make_float2(u0, v0);
  vector_float2 const uv1 = simd_make_float2(u1, v0);
  vector_float2 const uv2 = simd_make_float2(u0, v1);
  vector_float2 const uv3 = simd_make_float2(u1, v1);
  auto push = [&](Point const& p, vector_float2 uv) {
    MetalGlyphVertex gv{};
    gv.pos = simd_make_float2(p.x, p.y);
    gv.uv = uv;
    gv.color = premulRgba;
    out.push_back(gv);
  };
  push(p0, uv0);
  push(p1, uv1);
  push(p2, uv2);
  push(p1, uv1);
  push(p3, uv3);
  push(p2, uv2);
}

template <typename T>
void tagOpWithClip(T& op, bool clipValid, MTLScissorRect const& clip) {
  if (clipValid) {
    op.scissorValid = true;
    op.scissorX = static_cast<std::uint32_t>(clip.x);
    op.scissorY = static_cast<std::uint32_t>(clip.y);
    op.scissorW = static_cast<std::uint32_t>(clip.width);
    op.scissorH = static_cast<std::uint32_t>(clip.height);
  } else {
    op.scissorValid = false;
  }
}

void* retainTexturePointer(void* texture) {
  if (!texture) {
    return nullptr;
  }
  return (__bridge_retained void*)((__bridge id<MTLTexture>)texture);
}

template <typename T>
bool sameScissorForBatch(T const& a, T const& b) {
  if (a.scissorValid != b.scissorValid) {
    return false;
  }
  if (!a.scissorValid) {
    return true;
  }
  return a.scissorX == b.scissorX && a.scissorY == b.scissorY && a.scissorW == b.scissorW &&
         a.scissorH == b.scissorH;
}

template <typename T>
void setEncoderScissorForOp(id<MTLRenderCommandEncoder> enc, T const& op, MTLScissorRect fullScissor,
                            MTLScissorRect* last, bool* haveLast) {
  MTLScissorRect sc = fullScissor;
  if (op.scissorValid) {
    sc.x = op.scissorX;
    sc.y = op.scissorY;
    sc.width = op.scissorW;
    sc.height = op.scissorH;
  }
  if (!*haveLast || sc.x != last->x || sc.y != last->y || sc.width != last->width || sc.height != last->height) {
    [enc setScissorRect:sc];
    *last = sc;
    *haveLast = true;
  }
}

} // namespace

class MetalCanvas final : public Canvas {
public:
  MetalCanvas(Window* /*window*/, CAMetalLayer* layer, unsigned int handle, TextSystem& textSystem)
      : textSystem_(textSystem), metal_(layer), windowHandle_(handle) {
    glyphAtlas_ = std::make_unique<GlyphAtlas>(metal_.device(), textSystem_);
    glyphAtlas_->setBeforeGrowCallback([this]() {
      assert(frame_.glyphVerts.empty() &&
             "GlyphAtlas::grow() repacks UVs; cannot run while the frame still holds glyph vertices");
    });
    frameSem_ = dispatch_semaphore_create(static_cast<int>(kFramesInFlight));
    pushState();
  }

  ~MetalCanvas() override { frame_.clear(); }

  Backend backend() const noexcept override { return Backend::Metal; }

  unsigned int windowHandle() const override { return windowHandle_; }

  void resize(int width, int height) override {
    logicalW_ = width;
    logicalH_ = height;
  }

  void updateDpiScale(float scaleX, float scaleY) override {
    dpiScaleX_ = scaleX;
    dpiScaleY_ = scaleY;
    dpiScale_ = std::min(dpiScaleX_, dpiScaleY_);
  }

  void beginFrame() override {
    frame_.clear();
    // Each frame must start from a clean stack and identity transform. If a prior `render`
    // left extra states (e.g. unbalanced save/restore), transforms would accumulate and later
    // nodes (cards, subtitle) would draw off-screen or with wrong opacity.
    while (stateStack_.size() > 1) {
      stateStack_.pop_back();
    }
    if (!stateStack_.empty()) {
      stateStack_.back() = GpuState{};
      updateClipScissor();
    }
    dispatch_semaphore_wait(frameSem_, DISPATCH_TIME_FOREVER);
    metal_.advanceFrame();
    refreshFrameDrawableMetrics();
    drawable_ = [metal_.layer() nextDrawable];
    cmdBuf_ = [metal_.queue() commandBuffer];
    if (!drawable_) {
      dispatch_semaphore_signal(frameSem_);
      cmdBuf_ = nil;
      if (Application::hasInstance()) {
        Application::instance().requestRedraw();
      }
    }
    inFrame_ = (drawable_ != nil && cmdBuf_ != nil);
    const CGSize ds = frameDrawableSize_;
    CGFloat cs = metal_.layer().contentsScale;
    if (cs < 0.01) {
      cs = 1.0;
    }
    dpiScaleX_ = static_cast<float>(cs);
    dpiScaleY_ = static_cast<float>(cs);
    dpiScale_ = std::min(dpiScaleX_, dpiScaleY_);
    if (logicalW_ <= 0 || logicalH_ <= 0) {
      logicalW_ = static_cast<int>(std::lround(static_cast<double>(ds.width) / static_cast<double>(cs)));
      logicalH_ = static_cast<int>(std::lround(static_cast<double>(ds.height) / static_cast<double>(cs)));
    }
    if (inFrame_) {
      glyphAtlas_->prepareForFrameBegin();
    }
  }

  void clear(Color color) override { clearColor_ = color; }

  void setSyncPresent(bool sync) noexcept { syncPresent_ = sync; }

  void present() override {
    if (!inFrame_ || !cmdBuf_ || !drawable_) {
      syncPresent_ = false;
      return;
    }

    const float vw = frameDrawableW_;
    const float vh = frameDrawableH_;
    if (vw < 1.f || vh < 1.f) {
      syncPresent_ = false;
      [cmdBuf_ commit];
      cmdBuf_ = nil;
      drawable_ = nil;
      dispatch_semaphore_signal(frameSem_);
      inFrame_ = false;
      return;
    }

    metal_.uploadRectOps(frame_.rectOps);
    metal_.uploadImageOps(frame_.imageOps);
    metal_.uploadPathVertices(frame_.pathVerts);
    metal_.uploadGlyphVertices(frame_.glyphVerts);

    MTLRenderPassDescriptor* pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = drawable_.texture;
    pass.colorAttachments[0].loadAction = MTLLoadActionClear;
    pass.colorAttachments[0].clearColor =
        MTLClearColorMake(clearColor_.r, clearColor_.g, clearColor_.b, clearColor_.a);
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;

    id<MTLRenderCommandEncoder> enc = [cmdBuf_ renderCommandEncoderWithDescriptor:pass];
    MTLViewport vp = {0, 0, frameDrawableSize_.width, frameDrawableSize_.height, 0.0, 1.0};
    [enc setViewport:vp];

    MTLScissorRect const fullScissor = {0, 0, frameDrawablePixelsW_, frameDrawablePixelsH_};
    MTLScissorRect lastScissor = {0, 0, 0, 0};
    bool haveScissor = false;

    id<MTLBuffer> pathBuf = metal_.pathVertexArenaBuffer();
    std::size_t const opCount = frame_.opOrder.size();
    std::size_t i = 0;
    while (i < opCount) {
      MetalOpRef const ref = frame_.opOrder[i];
      if (ref.kind == MetalOpRef::Rect) {
        MetalRectOp const& op = frame_.rectOps[ref.index];
        std::size_t j = i + 1;
        while (j < opCount) {
          MetalOpRef const nextRef = frame_.opOrder[j];
          if (nextRef.kind != MetalOpRef::Rect) {
            break;
          }
          MetalRectOp const& o2 = frame_.rectOps[nextRef.index];
          MetalOpRef const prevRef = frame_.opOrder[j - 1];
          if (o2.isLine != op.isLine || o2.blendMode != op.blendMode || !sameScissorForBatch(op, o2) ||
              nextRef.index != prevRef.index + 1) {
            break;
          }
          ++j;
        }
        std::size_t const runLen = j - i;
        setEncoderScissorForOp(enc, op, fullScissor, &lastScissor, &haveScissor);
        if (!op.isLine) {
          [enc setRenderPipelineState:metal_.rectPSO(op.blendMode)];
          [enc setVertexBuffer:metal_.quadBuffer() offset:0 atIndex:0];
          const NSUInteger off = static_cast<NSUInteger>(ref.index) * sizeof(MetalRectInstance);
          [enc setVertexBuffer:metal_.instanceArenaBuffer() offset:off atIndex:1];
          [enc drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:kQuadStripCount
                  instanceCount:static_cast<NSUInteger>(runLen)];
        } else {
          [enc setRenderPipelineState:metal_.linePSO(op.blendMode)];
          [enc setVertexBuffer:metal_.quadBuffer() offset:0 atIndex:0];
          const NSUInteger off = static_cast<NSUInteger>(ref.index) * sizeof(MetalRectInstance);
          [enc setVertexBuffer:metal_.instanceArenaBuffer() offset:off atIndex:1];
          [enc drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:kQuadStripCount
                  instanceCount:static_cast<NSUInteger>(runLen)];
        }
        i = j;
        continue;
      }
      if (ref.kind == MetalOpRef::Glyph) {
        MetalGlyphOp const& op = frame_.glyphOps[ref.index];
        std::size_t j = i + 1;
        std::uint32_t runStart = op.glyphStart;
        std::uint32_t runVerts = op.glyphVertexCount;
        while (j < opCount) {
          MetalOpRef const nextRef = frame_.opOrder[j];
          if (nextRef.kind != MetalOpRef::Glyph) {
            break;
          }
          MetalGlyphOp const& o2 = frame_.glyphOps[nextRef.index];
          if (o2.blendMode != op.blendMode || !sameScissorForBatch(op, o2) ||
              o2.glyphStart != runStart + runVerts || nextRef.index != frame_.opOrder[j - 1].index + 1) {
            break;
          }
          runVerts += o2.glyphVertexCount;
          ++j;
        }
        setEncoderScissorForOp(enc, op, fullScissor, &lastScissor, &haveScissor);
        [enc setRenderPipelineState:metal_.glyphPSO(op.blendMode)];
        id<MTLBuffer> gbuf = metal_.glyphVertexArenaBuffer();
        const NSUInteger goff = static_cast<NSUInteger>(runStart) * sizeof(MetalGlyphVertex);
        [enc setVertexBuffer:gbuf offset:goff atIndex:0];
        float vp2[2] = {vw, vh};
        [enc setVertexBytes:vp2 length:sizeof(vp2) atIndex:1];
        [enc setFragmentTexture:glyphAtlas_->texture() atIndex:0];
        [enc setFragmentSamplerState:metal_.linearSampler() atIndex:0];
        [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:static_cast<NSUInteger>(runVerts)];
        i = j;
        continue;
      }

      if (ref.kind == MetalOpRef::Image) {
        MetalImageOp const& op = frame_.imageOps[ref.index];
        setEncoderScissorForOp(enc, op, fullScissor, &lastScissor, &haveScissor);
        if (!op.texture) {
          ++i;
          continue;
        }
        [enc setRenderPipelineState:metal_.imagePSO(op.blendMode)];
        [enc setVertexBuffer:metal_.quadBuffer() offset:0 atIndex:0];
        const NSUInteger off = static_cast<NSUInteger>(ref.index) * sizeof(MetalImageInstance);
        [enc setVertexBuffer:metal_.imageInstanceArenaBuffer() offset:off atIndex:1];
        [enc setFragmentTexture:(__bridge id<MTLTexture>)op.texture atIndex:0];
        [enc setFragmentSamplerState:op.repeatSampler ? metal_.repeatSampler() : metal_.linearSampler() atIndex:0];
        [enc drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:kQuadStripCount
                instanceCount:1];
        ++i;
        continue;
      }
      if (ref.kind == MetalOpRef::Path) {
        MetalPathOp const& op = frame_.pathOps[ref.index];
        setEncoderScissorForOp(enc, op, fullScissor, &lastScissor, &haveScissor);
        if (op.pathCount == 0) {
          ++i;
          continue;
        }
        [enc setRenderPipelineState:metal_.pathPSO(op.blendMode)];
        const NSUInteger off = static_cast<NSUInteger>(op.pathStart) * sizeof(PathVertex);
        [enc setVertexBuffer:pathBuf offset:off atIndex:0];
        [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:static_cast<NSUInteger>(op.pathCount)];
        ++i;
        continue;
      }
      assert(false && "unsupported Metal op ref kind");
    }

    [enc endEncoding];

    if (captureNextFrame_) {
      captureWidth_ = static_cast<std::uint32_t>(frameDrawablePixelsW_);
      captureHeight_ = static_cast<std::uint32_t>(frameDrawablePixelsH_);
      captureBytesPerRow_ = static_cast<NSUInteger>(captureWidth_) * 4U;
      captureBuffer_ = [metal_.device() newBufferWithLength:captureBytesPerRow_ * captureHeight_
                                                    options:MTLResourceStorageModeShared];
      if (captureBuffer_) {
        id<MTLBlitCommandEncoder> blit = [cmdBuf_ blitCommandEncoder];
        [blit copyFromTexture:drawable_.texture
                  sourceSlice:0
                  sourceLevel:0
                 sourceOrigin:MTLOriginMake(0, 0, 0)
                   sourceSize:MTLSizeMake(captureWidth_, captureHeight_, 1)
                     toBuffer:captureBuffer_
            destinationOffset:0
       destinationBytesPerRow:captureBytesPerRow_
     destinationBytesPerImage:captureBytesPerRow_ * captureHeight_];
        [blit endEncoding];
      }
      captureNextFrame_ = false;
    }

    if (syncPresent_) {
      lastSubmittedCmdBuf_ = cmdBuf_;
      [cmdBuf_ commit];
      [cmdBuf_ waitUntilScheduled];
      [drawable_ present];
      dispatch_semaphore_signal(frameSem_);
      syncPresent_ = false;
    } else {
      __block dispatch_semaphore_t sem = frameSem_;
      [cmdBuf_ addCompletedHandler:^(id<MTLCommandBuffer> /*cb*/) { dispatch_semaphore_signal(sem); }];
      [cmdBuf_ presentDrawable:drawable_];
      lastSubmittedCmdBuf_ = cmdBuf_;
      [cmdBuf_ commit];
    }

    frame_.clear();
    glyphAtlas_->afterPresent();

    cmdBuf_ = nil;
    drawable_ = nil;
    inFrame_ = false;
  }

  void save() override { pushState(); }

  void restore() override {
    if (stateStack_.size() <= 1) {
      return;
    }
    popState();
  }

  void setTransform(Mat3 const& m) override {
    currentState().transform = m;
    updateClipScissor();
  }

  void transform(Mat3 const& m) override {
    auto& st = currentState();
    if (st.clip.has_value()) {
      st.clip = boundsOfTransformedRect(*st.clip, m.inverse());
    }
    st.transform = st.transform * m;
    updateClipScissor();
  }

  void translate(Point offset) override { transform(Mat3::translate(offset)); }

  void translate(float x, float y) override { translate(Point{x, y}); }

  void scale(float sx, float sy) override { transform(Mat3::scale(sx, sy)); }

  void scale(float s) override { scale(s, s); }

  void rotate(float radians) override { transform(Mat3::rotate(radians)); }

  void rotate(float radians, Point pivot) override { transform(Mat3::rotate(radians, pivot)); }

  Mat3 currentTransform() const override { return currentState().transform; }

  void clipRect(Rect rect, bool /*antiAlias*/) override {
    auto& st = currentState();
    if (!st.clip.has_value()) {
      st.clip = rect;
    } else {
      st.clip = intersectRects(*st.clip, rect);
    }
    updateClipScissor();
  }

  Rect clipBounds() const override {
    if (stateStack_.empty()) {
      return viewportLogicalRect();
    }
    const auto& st = stateStack_.back();
    if (st.clip.has_value()) {
      return *st.clip;
    }
    return viewportLogicalRect();
  }

  bool quickReject(Rect rect) const override {
    if (!currentState().clip.has_value()) {
      return false;
    }
    // `rect` and `st.clip` are both in the current local coordinate system (same as drawRect).
    // Do not compare transformed bounds to an axis-aligned clip in local space.
    return !intersects(rect, *currentState().clip);
  }

  void setOpacity(float o) override { currentState().opacity = std::clamp(o, 0.f, 1.f); }

  float opacity() const override { return currentState().opacity; }

  void setBlendMode(BlendMode mode) override { currentState().blendMode = mode; }

  BlendMode blendMode() const override { return currentState().blendMode; }

  void drawRect(Rect const& rect, CornerRadius const& cornerRadius, FillStyle const& fs,
                StrokeStyle const& ss, ShadowStyle const& shadow) override {
    if (!inFrame_) {
      return;
    }
    Color fillC{};
    Color strokeC{};
    bool hasFill = false;
    bool hasStroke = false;
    if (!fs.isNone() && fs.solidColor(&fillC)) {
      hasFill = true;
    }
    if (!ss.isNone() && ss.solidColor(&strokeC)) {
      hasStroke = true;
    }
    if (!hasFill && !hasStroke && shadow.isNone()) {
      return;
    }
    if (quickReject(rect)) {
      return;
    }
    const float op = effectiveOpacity();
    Mat3 const& M = currentState().transform;
    Rect drawR = rect;
    if (currentState().clip.has_value()) {
      Rect const inter = intersectRects(rect, *currentState().clip);
      if (inter.width <= 0.f || inter.height <= 0.f) {
        return;
      }
      drawR = inter;
    }

    CornerRadius crEffective = cornerRadius;
    {
      constexpr float eps = 1e-3f;
      bool const clipped =
          std::abs(drawR.x - rect.x) > eps || std::abs(drawR.y - rect.y) > eps ||
          std::abs(drawR.width - rect.width) > eps || std::abs(drawR.height - rect.height) > eps;
      if (clipped) {
        crEffective = cornerRadiiAfterAxisAlignedClip(rect, drawR, cornerRadius);
      }
      clampRoundRectCornerRadii(drawR.width, drawR.height, crEffective);
    }

    float rotationRad = 0.f;
    Rect mapped{};
    if (M.isTranslationOnly()) {
      mapped = Rect::sharp(drawR.x + M.m[6], drawR.y + M.m[7], drawR.width, drawR.height);
      rotationRad = 0.f;
    } else {
      float tx = 0.f;
      float ty = 0.f;
      bool const decomposed = tryDecomposeRotationTranslation(M, &rotationRad, &tx, &ty);
      mapped = boundsOfTransformedRect(drawR, M);
      if (decomposed) {
        rotationRad = 0.f;
      }
    }

    const float s = dpiScale_;
    CornerRadius cr{};
    cr.topLeft = crEffective.topLeft * s;
    cr.topRight = crEffective.topRight * s;
    cr.bottomRight = crEffective.bottomRight * s;
    cr.bottomLeft = crEffective.bottomLeft * s;
    Rect device = Rect::sharp(mapped.x * dpiScaleX_, mapped.y * dpiScaleY_, mapped.width * dpiScaleX_,
                              mapped.height * dpiScaleY_);
    Color shadowC{};
    float shadowOx = 0.f;
    float shadowOy = 0.f;
    float shadowR = 0.f;
    if (!shadow.isNone()) {
      shadowC = shadow.color;
      shadowC.a *= op;
      shadowOx = shadow.offset.x * dpiScaleX_;
      shadowOy = shadow.offset.y * dpiScaleY_;
      shadowR = shadow.radius * s;
    }
    emitRect(device, cr, hasFill ? fillC : Color{0, 0, 0, 0}, hasStroke ? strokeC : Color{0, 0, 0, 0},
             hasStroke ? ss.width * s : 0.f, op, rotationRad, shadowC, shadowOx, shadowOy, shadowR);
  }

  void drawLine(Point a, Point b, StrokeStyle const& ss) override {
    if (!inFrame_) {
      return;
    }
    Color stroke{};
    if (!ss.solidColor(&stroke)) {
      return;
    }
    const float paintOpacity = effectiveOpacity();
    const float pad = std::max(ss.width * 2.f, 4.f);
    if (currentState().clip.has_value()) {
      const float minX = std::min(a.x, b.x) - pad;
      const float maxX = std::max(a.x, b.x) + pad;
      const float minY = std::min(a.y, b.y) - pad;
      const float maxY = std::max(a.y, b.y) + pad;
      Rect const lineBoundsLocal = Rect::sharp(minX, minY, maxX - minX, maxY - minY);
      if (!intersects(lineBoundsLocal, *currentState().clip)) {
        return;
      }
    }
    Point ta{};
    Point tb{};
    if (currentState().transform.isTranslationOnly()) {
      ta = Point{a.x + currentState().transform.m[6], a.y + currentState().transform.m[7]};
      tb = Point{b.x + currentState().transform.m[6], b.y + currentState().transform.m[7]};
    } else {
      ta = currentState().transform.apply(a);
      tb = currentState().transform.apply(b);
    }
    const float dx = tb.x - ta.x;
    const float dy = tb.y - ta.y;
    const float len = std::sqrt(dx * dx + dy * dy);
    // Reject only true degenerates. A threshold like 1e-4f dropped vertical carets when
    // `line.bottom - line.top` was tiny-but-positive (Core Text / float noise) or subpixel after transform.
    if (!std::isfinite(len) || len <= 0.f) {
      return;
    }
    const float w = len + pad * 2.f;
    const float h = ss.width + pad * 2.f;
    const float cx = (ta.x + tb.x) * 0.5f;
    const float cy = (ta.y + tb.y) * 0.5f;
    Rect const lineBounds = Rect::sharp(cx - w * 0.5f, cy - h * 0.5f, w, h);

    MetalRectOp op{};
    op.isLine = true;
    op.inst.rect = simd_make_float4(lineBounds.x * dpiScaleX_, lineBounds.y * dpiScaleY_,
                                    lineBounds.width * dpiScaleX_, lineBounds.height * dpiScaleY_);
    const float inv = 1.f / len;
    const float lenDevice = std::hypot(dx * dpiScaleX_, dy * dpiScaleY_);
    op.inst.corners = simd_make_float4(dx * inv, dy * inv, lenDevice * 0.5f, 0.f);
    op.inst.fillColor = simd_make_float4(0.f, 0.f, 0.f, 0.f);
    op.inst.strokeColor = toSimd4(stroke);
    op.inst.strokeWidthOpacity =
        simd_make_float2(ss.width * dpiScale_, paintOpacity);
    op.inst.viewport = simd_make_float2(frameDrawableW_, frameDrawableH_);
    op.inst.rotationPad = simd_make_float4(0.f, 0.f, 0.f, 0.f);
    op.inst.shadowColor = simd_make_float4(0.f, 0.f, 0.f, 0.f);
    op.inst.shadowGeom = simd_make_float4(0.f, 0.f, 0.f, 0.f);
    op.blendMode = currentState().blendMode;
    pushRectOp(std::move(op));
  }

  void drawPath(Path const& path, FillStyle const& fs, StrokeStyle const& ss,
                ShadowStyle const& shadow) override {
    if (!inFrame_ || path.isEmpty()) {
      return;
    }
    MetalFrameRecorder& recorder = activeRecorder();

    if (path.commandCount() == 1) {
      Path::CommandView cv = path.command(0);
      if (cv.type == Path::CommandType::Rect && cv.dataCount >= 8) {
        const float* d = cv.data;
        Rect r{d[0], d[1], d[2], d[3]};
        CornerRadius cr{d[4], d[5], d[6], d[7]};
        drawRect(r, cr, fs, ss, shadow);
        return;
      }
      const bool circlePrim = cv.type == Path::CommandType::Circle && cv.dataCount >= 3;
      const bool ellipsePrim = cv.type == Path::CommandType::Ellipse && cv.dataCount >= 4;
      if (circlePrim || ellipsePrim) {
        Color fc{};
        if (!fs.isNone() && fs.solidColor(&fc)) {
          if (circlePrim) {
            float const rad = cv.data[2];
            Rect r{cv.data[0] - rad, cv.data[1] - rad, rad * 2.f, rad * 2.f};
            drawRect(r, CornerRadius::pill(r), fs, ss, shadow);
          } else {
            float const rx = cv.data[2];
            float const ry = cv.data[3];
            Rect r{cv.data[0] - rx, cv.data[1] - ry, rx * 2.f, ry * 2.f};
            drawRect(r, CornerRadius::pill(r), fs, ss, shadow);
          }
          return;
        }
      }
    }

    // Round stroke on open polylines: `drawLine` uses the capsule SDF (same rounded look as clock
    // hands). CPU path-mesh stroke expansion is a separate pipeline and does not match that shader.
    if (fs.isNone() && ss.cap == StrokeCap::Round && ss.join == StrokeJoin::Round && pathIsMoveLineOnlyStroke(path)) {
      Color sc{};
      if (ss.solidColor(&sc)) {
        auto subpaths = PathFlattener::flattenSubpaths(path);
        std::size_t const nOpsBefore = recorder.opOrder.size();
        for (auto const& sp : subpaths) {
          if (sp.size() < 2) {
            continue;
          }
          for (size_t i = 0; i + 1 < sp.size(); ++i) {
            drawLine(sp[i], sp[i + 1], ss);
          }
        }
        if (recorder.opOrder.size() > nOpsBefore) {
          return;
        }
      }
    }

    // Mesh path: draw translated fill as drop shadow (no SDF blur; matches rect shadow intent via offset).
    if (!shadow.isNone()) {
      Color fillProbe{};
      if (!fs.isNone() && fs.solidColor(&fillProbe)) {
        (void)fillProbe;
        save();
        translate(shadow.offset.x, shadow.offset.y);
        std::size_t const nShadow = recorder.pathOps.size();
        metalPathRasterizeToMesh(path, FillStyle::solid(shadow.color), StrokeStyle::none(), currentState().transform,
                                 dpiScaleX_, dpiScaleY_, effectiveOpacity(), frameDrawableW_, frameDrawableH_,
                                 recorder.pathVerts, recorder.pathOps, recorder.opOrder,
                                 currentState().blendMode);
        if (recorder.pathOps.size() > nShadow) {
          tagOpWithClip(recorder.pathOps.back(), clipScissorValid_, clipScissor_);
        }
        restore();
      }
    }

    std::size_t const nOpsBefore = recorder.pathOps.size();
    metalPathRasterizeToMesh(path, fs, ss, currentState().transform, dpiScaleX_, dpiScaleY_, effectiveOpacity(),
                             frameDrawableW_, frameDrawableH_, recorder.pathVerts, recorder.pathOps,
                             recorder.opOrder,
                             currentState().blendMode);
    if (recorder.pathOps.size() > nOpsBefore) {
      tagOpWithClip(recorder.pathOps.back(), clipScissorValid_, clipScissor_);
    }
  }

  void drawCircle(Point center, float radius, FillStyle const& fs, StrokeStyle const& ss) override {
    Rect r{center.x - radius, center.y - radius, radius * 2.f, radius * 2.f};
    drawRect(r, CornerRadius::pill(r), fs, ss, ShadowStyle::none());
  }

  void drawImage(Image const& image, Rect const& src, Rect const& dst, CornerRadius const& corners,
                 float opacity) override {
    if (!inFrame_) {
      return;
    }
    MetalImage const* mh = tryMetalImage(image);
    if (!mh || !mh->texture()) {
      return;
    }
    Size const sz = image.size();
    if (sz.width <= 0.f || sz.height <= 0.f) {
      return;
    }
    if (src.width <= 0.f || src.height <= 0.f || dst.width <= 0.f || dst.height <= 0.f) {
      return;
    }
    if (quickReject(dst)) {
      return;
    }
    Mat3 const& M = currentState().transform;
    Rect const mappedClip = boundsOfTransformedRect(dst, M);
    if (currentState().clip.has_value()) {
      Rect const inter = intersectRects(dst, *currentState().clip);
      if (inter.width <= 0.f || inter.height <= 0.f) {
        return;
      }
    }

    Rect mapped{};
    float rotationRad = 0.f;
    if (M.isTranslationOnly()) {
      mapped = Rect::sharp(dst.x + M.m[6], dst.y + M.m[7], dst.width, dst.height);
    } else {
      if (currentState().clip.has_value()) {
        Rect const clipped = intersectRects(dst, *currentState().clip);
        if (clipped.width <= 0.f || clipped.height <= 0.f) {
          return;
        }
        mapped = boundsOfTransformedRect(clipped, M);
      } else {
        mapped = mappedClip;
      }
    }

    const float s = dpiScale_;
    CornerRadius cr{};
    cr.topLeft = corners.topLeft * s;
    cr.topRight = corners.topRight * s;
    cr.bottomRight = corners.bottomRight * s;
    cr.bottomLeft = corners.bottomLeft * s;
    Rect device = Rect::sharp(mapped.x * dpiScaleX_, mapped.y * dpiScaleY_, mapped.width * dpiScaleX_,
                              mapped.height * dpiScaleY_);

    const float op = effectiveOpacity() * opacity;
    float const iw = sz.width;
    float const ih = sz.height;
    float const u0 = src.x / iw;
    float const v0 = src.y / ih;
    float const u1 = (src.x + src.width) / iw;
    float const v1 = (src.y + src.height) / ih;

    emitImage(mh->texture(), device, cr, simd_make_float4(u0, v0, u1, v1), simd_make_float2(0.f, 0.f), 0.f, op,
              rotationRad, false);
  }

  void drawImageTiled(Image const& image, Rect const& dst, CornerRadius const& corners, float opacity) override {
    if (!inFrame_) {
      return;
    }
    MetalImage const* mh = tryMetalImage(image);
    if (!mh || !mh->texture()) {
      return;
    }
    Size const sz = image.size();
    if (sz.width <= 0.f || sz.height <= 0.f) {
      return;
    }
    if (dst.width <= 0.f || dst.height <= 0.f) {
      return;
    }
    if (quickReject(dst)) {
      return;
    }
    Mat3 const& M = currentState().transform;
    Rect const mappedClip = boundsOfTransformedRect(dst, M);
    if (currentState().clip.has_value()) {
      Rect const inter = intersectRects(dst, *currentState().clip);
      if (inter.width <= 0.f || inter.height <= 0.f) {
        return;
      }
    }

    Rect mapped{};
    float rotationRad = 0.f;
    if (M.isTranslationOnly()) {
      mapped = Rect::sharp(dst.x + M.m[6], dst.y + M.m[7], dst.width, dst.height);
    } else {
      if (currentState().clip.has_value()) {
        Rect const clipped = intersectRects(dst, *currentState().clip);
        if (clipped.width <= 0.f || clipped.height <= 0.f) {
          return;
        }
        mapped = boundsOfTransformedRect(clipped, M);
      } else {
        mapped = mappedClip;
      }
    }

    const float s = dpiScale_;
    CornerRadius cr{};
    cr.topLeft = corners.topLeft * s;
    cr.topRight = corners.topRight * s;
    cr.bottomRight = corners.bottomRight * s;
    cr.bottomLeft = corners.bottomLeft * s;
    Rect device = Rect::sharp(mapped.x * dpiScaleX_, mapped.y * dpiScaleY_, mapped.width * dpiScaleX_,
                              mapped.height * dpiScaleY_);

    const float op = effectiveOpacity() * opacity;
    vector_float2 const texInv = simd_make_float2(1.f / sz.width, 1.f / sz.height);
    emitImage(mh->texture(), device, cr, simd_make_float4(0.f, 0.f, 0.f, 0.f), texInv, 1.f, op, rotationRad, true);
  }

  void drawTextLayout(TextLayout const& layout, Point origin) override {
    if (!inFrame_) {
      return;
    }
    MetalFrameRecorder& recorder = activeRecorder();

    Mat3 const& M = currentState().transform;
    BlendMode const blend = currentState().blendMode;
    float const op = effectiveOpacity();

    float const aw = static_cast<float>(glyphAtlas_->atlasPixelWidth());
    float const ah = static_cast<float>(glyphAtlas_->atlasPixelHeight());
    if (aw < 1.f || ah < 1.f) {
      return;
    }
    float const invAw = 1.f / aw;
    float const invAh = 1.f / ah;
    float const invDpiX = 1.f / dpiScaleX_;
    float const invDpiY = 1.f / dpiScaleY_;

    std::uint32_t const glyphStart = static_cast<std::uint32_t>(recorder.glyphVerts.size());
    for (auto const& placed : layout.runs) {
      TextRun const& text = placed.run;
      float const baselineY = origin.y + placed.origin.y;
      float const x = origin.x + placed.origin.x;

      if (text.backgroundColor.has_value() && text.width > 0.f) {
        drawRect(Rect{x, baselineY - text.ascent, text.width, text.ascent + text.descent}, CornerRadius{}, FillStyle::solid(*text.backgroundColor), StrokeStyle::none(), ShadowStyle::none());
      }

      if (text.glyphIds.empty()) {
        continue;
      }

      float const effectiveAlpha = text.color.a * op;
      vector_float4 const premul = simd_make_float4(text.color.r * effectiveAlpha,
                                                    text.color.g * effectiveAlpha,
                                                    text.color.b * effectiveAlpha,
                                                    effectiveAlpha);

      float const physicalFontSize = text.fontSize * dpiScaleX_;

      std::size_t const glyphCount = std::min(text.glyphIds.size(), text.positions.size());
      for (std::size_t i = 0; i < glyphCount; ++i) {
        GlyphKey key{};
        key.fontId = text.fontId;
        key.glyphId = text.glyphIds[i];
        unsigned const q = static_cast<unsigned>(physicalFontSize * 4.f);
        key.sizeQ8 = static_cast<std::uint16_t>(std::min(65535u, q));

        AtlasEntry const& entry = glyphAtlas_->getOrUpload(key);
        if (entry.width == 0 || entry.height == 0) {
          continue;
        }

        float const u0 = static_cast<float>(entry.u) * invAw;
        float const u1 = static_cast<float>(entry.u + entry.width) * invAw;
        float const vLo = static_cast<float>(entry.v) * invAh;
        float const vHi = static_cast<float>(entry.v + entry.height) * invAh;

        Point const ink = {x + text.positions[i].x, baselineY + text.positions[i].y};
        Point const tl = {ink.x - entry.bearing.x * invDpiX, ink.y - entry.bearing.y * invDpiY};
        float const gw = static_cast<float>(entry.width) * invDpiX;
        float const gh = static_cast<float>(entry.height) * invDpiY;

        appendGlyphQuad(recorder.glyphVerts, M, dpiScaleX_, dpiScaleY_, tl, gw, gh, u0, vLo, u1, vHi, premul);
      }
    }

    std::uint32_t const vertCount = static_cast<std::uint32_t>(recorder.glyphVerts.size()) - glyphStart;
    if (vertCount > 0) {
      MetalGlyphOp op{};
      op.glyphStart = glyphStart;
      op.glyphVertexCount = vertCount;
      op.blendMode = blend;
      pushGlyphOp(std::move(op));
    }
  }

  void* gpuDevice() const override { return (__bridge void*)metal_.device(); }

private:
  struct GpuState {
    Mat3 transform = Mat3::identity();
    float opacity = 1.f;
    BlendMode blendMode = BlendMode::Normal;
    std::optional<Rect> clip;
  };

  TextSystem& textSystem_;
  std::unique_ptr<GlyphAtlas> glyphAtlas_;
  MetalDeviceResources metal_;
  unsigned int windowHandle_{0};

  dispatch_semaphore_t frameSem_{nullptr};
  id<MTLCommandBuffer> cmdBuf_{nil};
  id<MTLCommandBuffer> lastSubmittedCmdBuf_{nil};
  id<CAMetalDrawable> drawable_{nil};
  id<MTLBuffer> captureBuffer_{nil};
  NSUInteger captureBytesPerRow_{0};
  std::uint32_t captureWidth_{0};
  std::uint32_t captureHeight_{0};
  bool captureNextFrame_{false};
  bool inFrame_{false};
  bool syncPresent_{false};

  Color clearColor_{0.f, 0.f, 0.f, 1.f};
  int logicalW_{0};
  int logicalH_{0};
  float dpiScaleX_{1.f};
  float dpiScaleY_{1.f};
  float dpiScale_{1.f};
  CGSize frameDrawableSize_{};
  float frameDrawableW_{0.f};
  float frameDrawableH_{0.f};
  NSUInteger frameDrawablePixelsW_{0};
  NSUInteger frameDrawablePixelsH_{0};

  MetalFrameRecorder frame_;
  MetalFrameRecorder* captureRecorder_{nullptr};
  std::vector<GpuState> stateStack_;

  MTLScissorRect clipScissor_{};
  bool clipScissorValid_{false};

  GpuState& currentState() { return stateStack_.back(); }
  GpuState const& currentState() const { return stateStack_.back(); }
  MetalFrameRecorder& activeRecorder() { return captureRecorder_ ? *captureRecorder_ : frame_; }
  MetalFrameRecorder const& activeRecorder() const { return captureRecorder_ ? *captureRecorder_ : frame_; }

  void pushState() { stateStack_.push_back(stateStack_.empty() ? GpuState{} : stateStack_.back()); }

  void popState() {
    stateStack_.pop_back();
    updateClipScissor();
  }

  float effectiveOpacity() const { return currentState().opacity; }

  void refreshFrameDrawableMetrics() {
    CGSize const ds = metal_.layer().drawableSize;
    frameDrawableSize_ = ds;
    frameDrawableW_ = static_cast<float>(ds.width);
    frameDrawableH_ = static_cast<float>(ds.height);
    frameDrawablePixelsW_ = static_cast<NSUInteger>(ds.width);
    frameDrawablePixelsH_ = static_cast<NSUInteger>(ds.height);
  }

  Rect viewportLogicalRect() const {
    if (logicalW_ > 0 && logicalH_ > 0) {
      return Rect::sharp(0, 0, static_cast<float>(logicalW_), static_cast<float>(logicalH_));
    }
    CGSize ds = inFrame_ ? frameDrawableSize_ : metal_.layer().drawableSize;
    return Rect::sharp(0, 0, static_cast<float>(ds.width) / dpiScaleX_, static_cast<float>(ds.height) / dpiScaleY_);
  }

  void updateClipScissor() {
    if (!currentState().clip.has_value()) {
      clipScissorValid_ = false;
      return;
    }
    Mat3 const& M = currentState().transform;
    Rect const world = boundsOfTransformedRect(*currentState().clip, M);
    float const minX = world.x * dpiScaleX_;
    float const minY = world.y * dpiScaleY_;
    float const maxX = (world.x + world.width) * dpiScaleX_;
    float const maxY = (world.y + world.height) * dpiScaleY_;
    NSUInteger const dw = inFrame_ ? frameDrawablePixelsW_ : static_cast<NSUInteger>(metal_.layer().drawableSize.width);
    NSUInteger const dh = inFrame_ ? frameDrawablePixelsH_ : static_cast<NSUInteger>(metal_.layer().drawableSize.height);
    float const clampedMinX = std::clamp(minX, 0.f, static_cast<float>(dw));
    float const clampedMinY = std::clamp(minY, 0.f, static_cast<float>(dh));
    float const clampedMaxX = std::clamp(maxX, 0.f, static_cast<float>(dw));
    float const clampedMaxY = std::clamp(maxY, 0.f, static_cast<float>(dh));

    // Expand to the covered pixel envelope instead of truncating. This avoids
    // thin or partially clipped primitives blinking out when their visible
    // extent is subpixel but still non-zero in logical space.
    NSUInteger const x0 = static_cast<NSUInteger>(std::floor(clampedMinX));
    NSUInteger const y0 = static_cast<NSUInteger>(std::floor(clampedMinY));
    NSUInteger const x1 = static_cast<NSUInteger>(std::ceil(clampedMaxX));
    NSUInteger const y1 = static_cast<NSUInteger>(std::ceil(clampedMaxY));
    clipScissor_.x = std::min(x0, dw);
    clipScissor_.y = std::min(y0, dh);
    clipScissor_.width = x1 > clipScissor_.x ? std::min(x1 - clipScissor_.x, dw - clipScissor_.x) : 0;
    clipScissor_.height = y1 > clipScissor_.y ? std::min(y1 - clipScissor_.y, dh - clipScissor_.y) : 0;
    clipScissorValid_ = clipScissor_.width > 0 && clipScissor_.height > 0;
  }

  void pushRectOp(MetalRectOp&& op) {
    MetalFrameRecorder& recorder = activeRecorder();
    tagOpWithClip(op, clipScissorValid_, clipScissor_);
    recorder.opOrder.push_back(MetalOpRef{
        .kind = MetalOpRef::Rect,
        .index = static_cast<std::uint32_t>(recorder.rectOps.size()),
    });
    recorder.rectOps.push_back(std::move(op));
  }

  void pushImageOp(MetalImageOp&& op) {
    MetalFrameRecorder& recorder = activeRecorder();
    tagOpWithClip(op, clipScissorValid_, clipScissor_);
    recorder.opOrder.push_back(MetalOpRef{
        .kind = MetalOpRef::Image,
        .index = static_cast<std::uint32_t>(recorder.imageOps.size()),
    });
    recorder.imageOps.push_back(std::move(op));
  }

  void pushGlyphOp(MetalGlyphOp&& op) {
    MetalFrameRecorder& recorder = activeRecorder();
    tagOpWithClip(op, clipScissorValid_, clipScissor_);
    recorder.opOrder.push_back(MetalOpRef{
        .kind = MetalOpRef::Glyph,
        .index = static_cast<std::uint32_t>(recorder.glyphOps.size()),
    });
    recorder.glyphOps.push_back(std::move(op));
  }

  void emitRect(Rect const& deviceRect, CornerRadius const& corners, Color const& fillColor, Color const& strokeColor,
                float strokeWidth, float opacity, float rotationRad, Color const& shadowColor, float shadowOffsetX,
                float shadowOffsetY, float shadowRadius) {
    MetalRectOp op{};
    op.inst.rect = simd_make_float4(deviceRect.x, deviceRect.y, deviceRect.width, deviceRect.height);
    op.inst.corners = cornersToSimd(corners);
    op.inst.fillColor = toSimd4(fillColor);
    op.inst.strokeColor = toSimd4(strokeColor);
    op.inst.strokeWidthOpacity = simd_make_float2(strokeWidth, opacity);
    op.inst.viewport = simd_make_float2(frameDrawableW_, frameDrawableH_);
    op.inst.rotationPad = simd_make_float4(rotationRad, 0.f, 0.f, 0.f);
    op.inst.shadowColor = toSimd4(shadowColor);
    op.inst.shadowGeom = simd_make_float4(shadowOffsetX, shadowOffsetY, shadowRadius, 0.f);
    op.blendMode = currentState().blendMode;
    pushRectOp(std::move(op));
  }

  void emitImage(id<MTLTexture> tex, Rect const& deviceRect, CornerRadius const& corners, vector_float4 const& uvBounds,
                 vector_float2 const& texSizeInv, float imageMode, float opacity, float rotationRad, bool repeat) {
    MetalImageOp op{};
    op.inst.sdf.rect = simd_make_float4(deviceRect.x, deviceRect.y, deviceRect.width, deviceRect.height);
    op.inst.sdf.corners = cornersToSimd(corners);
    op.inst.sdf.fillColor = simd_make_float4(1.f, 1.f, 1.f, 1.f);
    op.inst.sdf.strokeColor = simd_make_float4(0.f, 0.f, 0.f, 0.f);
    op.inst.sdf.strokeWidthOpacity = simd_make_float2(0.f, opacity);
    op.inst.sdf.viewport = simd_make_float2(frameDrawableW_, frameDrawableH_);
    op.inst.sdf.rotationPad = simd_make_float4(rotationRad, 0.f, 0.f, 0.f);
    op.inst.sdf.shadowColor = simd_make_float4(0.f, 0.f, 0.f, 0.f);
    op.inst.sdf.shadowGeom = simd_make_float4(0.f, 0.f, 0.f, 0.f);
    op.inst.uvBounds = uvBounds;
    op.inst.texSizeInv = texSizeInv;
    op.inst.imageModePad = simd_make_float2(imageMode, 0.f);
    op.blendMode = currentState().blendMode;
    op.texture = (__bridge_retained void*)tex;
    op.repeatSampler = repeat;
    pushImageOp(std::move(op));
  }

public:
  void beginRecordedOpsCapture(MetalFrameRecorder* target) { captureRecorder_ = target; }

  void endRecordedOpsCapture() { captureRecorder_ = nullptr; }

  void replayRecordedOps(MetalFrameRecorder const& recorded, MetalRecorderSlice const& slice) {
    std::uint32_t const framePathVertexBase = static_cast<std::uint32_t>(frame_.pathVerts.size());
    std::uint32_t const frameGlyphVertexBase = static_cast<std::uint32_t>(frame_.glyphVerts.size());
    std::uint32_t const frameRectBase = static_cast<std::uint32_t>(frame_.rectOps.size());
    std::uint32_t const frameImageBase = static_cast<std::uint32_t>(frame_.imageOps.size());
    std::uint32_t const framePathOpBase = static_cast<std::uint32_t>(frame_.pathOps.size());
    std::uint32_t const frameGlyphOpBase = static_cast<std::uint32_t>(frame_.glyphOps.size());

    if (slice.pathVertexCount > 0) {
      frame_.pathVerts.insert(frame_.pathVerts.end(),
                              recorded.pathVerts.begin() + static_cast<std::ptrdiff_t>(slice.pathVertexStart),
                              recorded.pathVerts.begin() +
                                  static_cast<std::ptrdiff_t>(slice.pathVertexStart + slice.pathVertexCount));
    }
    if (slice.glyphVertexCount > 0) {
      frame_.glyphVerts.insert(frame_.glyphVerts.end(),
                               recorded.glyphVerts.begin() + static_cast<std::ptrdiff_t>(slice.glyphVertexStart),
                               recorded.glyphVerts.begin() +
                                   static_cast<std::ptrdiff_t>(slice.glyphVertexStart + slice.glyphVertexCount));
    }
    if (slice.rectCount > 0) {
      frame_.rectOps.insert(frame_.rectOps.end(),
                            recorded.rectOps.begin() + static_cast<std::ptrdiff_t>(slice.rectStart),
                            recorded.rectOps.begin() + static_cast<std::ptrdiff_t>(slice.rectStart + slice.rectCount));
    }
    if (slice.imageCount > 0) {
      frame_.imageOps.insert(frame_.imageOps.end(),
                             recorded.imageOps.begin() + static_cast<std::ptrdiff_t>(slice.imageStart),
                             recorded.imageOps.begin() +
                                 static_cast<std::ptrdiff_t>(slice.imageStart + slice.imageCount));
      for (std::uint32_t i = 0; i < slice.imageCount; ++i) {
        MetalImageOp& op = frame_.imageOps[frameImageBase + static_cast<std::size_t>(i)];
        if (op.texture) {
          op.texture = retainTexturePointer(op.texture);
        }
      }
    }
    if (slice.pathOpCount > 0) {
      frame_.pathOps.insert(frame_.pathOps.end(),
                            recorded.pathOps.begin() + static_cast<std::ptrdiff_t>(slice.pathOpStart),
                            recorded.pathOps.begin() +
                                static_cast<std::ptrdiff_t>(slice.pathOpStart + slice.pathOpCount));
      for (std::uint32_t i = 0; i < slice.pathOpCount; ++i) {
        MetalPathOp& op = frame_.pathOps[framePathOpBase + static_cast<std::size_t>(i)];
        op.pathStart = framePathVertexBase + (op.pathStart - slice.pathVertexStart);
      }
    }
    if (slice.glyphOpCount > 0) {
      frame_.glyphOps.insert(frame_.glyphOps.end(),
                             recorded.glyphOps.begin() + static_cast<std::ptrdiff_t>(slice.glyphOpStart),
                             recorded.glyphOps.begin() +
                                 static_cast<std::ptrdiff_t>(slice.glyphOpStart + slice.glyphOpCount));
      for (std::uint32_t i = 0; i < slice.glyphOpCount; ++i) {
        MetalGlyphOp& op = frame_.glyphOps[frameGlyphOpBase + static_cast<std::size_t>(i)];
        op.glyphStart = frameGlyphVertexBase + (op.glyphStart - slice.glyphVertexStart);
      }
    }
    if (slice.orderCount > 0) {
      frame_.opOrder.insert(frame_.opOrder.end(),
                            recorded.opOrder.begin() + static_cast<std::ptrdiff_t>(slice.orderStart),
                            recorded.opOrder.begin() + static_cast<std::ptrdiff_t>(slice.orderStart + slice.orderCount));
      for (std::uint32_t i = 0; i < slice.orderCount; ++i) {
        MetalOpRef& ref = frame_.opOrder[frame_.opOrder.size() - slice.orderCount + i];
        switch (ref.kind) {
        case MetalOpRef::Rect:
          ref.index = frameRectBase + (ref.index - slice.rectStart);
          break;
        case MetalOpRef::Image:
          ref.index = frameImageBase + (ref.index - slice.imageStart);
          break;
        case MetalOpRef::Path:
          ref.index = framePathOpBase + (ref.index - slice.pathOpStart);
          break;
        case MetalOpRef::Glyph:
          ref.index = frameGlyphOpBase + (ref.index - slice.glyphOpStart);
          break;
        }
      }
    }
  }

  void waitForLastPresentComplete() {
    if (!lastSubmittedCmdBuf_) {
      return;
    }
    [lastSubmittedCmdBuf_ waitUntilCompleted];
  }

  void requestNextFrameCapture() { captureNextFrame_ = true; }

  bool takeCapturedFrame(std::vector<std::uint8_t>& out, std::uint32_t& width, std::uint32_t& height) {
    if (!captureBuffer_ || captureWidth_ == 0 || captureHeight_ == 0) {
      return false;
    }
    std::size_t const size = static_cast<std::size_t>(captureBytesPerRow_) * captureHeight_;
    out.resize(size);
    std::memcpy(out.data(), [captureBuffer_ contents], size);
    width = captureWidth_;
    height = captureHeight_;
    captureBuffer_ = nil;
    captureBytesPerRow_ = 0;
    captureWidth_ = 0;
    captureHeight_ = 0;
    return true;
  }
};

std::unique_ptr<Canvas> createMetalCanvas(Window* window, void* caMetalLayer, unsigned int handle,
                                          TextSystem& textSystem) {
  return std::make_unique<MetalCanvas>(window, (__bridge CAMetalLayer*)caMetalLayer, handle, textSystem);
}

void setSyncPresentForCanvas(Canvas* canvas, bool sync) {
  if (!canvas) {
    return;
  }
  if (auto* mc = dynamic_cast<MetalCanvas*>(canvas)) {
    mc->setSyncPresent(sync);
  }
}

void waitForCanvasLastPresentComplete(Canvas* canvas) {
  if (!canvas) {
    return;
  }
  if (auto* mc = dynamic_cast<MetalCanvas*>(canvas)) {
    mc->waitForLastPresentComplete();
  }
}

bool beginRecordedOpsCaptureForCanvas(Canvas* canvas, MetalFrameRecorder* target) {
  if (!canvas || !target) {
    return false;
  }
  if (auto* mc = dynamic_cast<MetalCanvas*>(canvas)) {
    mc->beginRecordedOpsCapture(target);
    return true;
  }
  return false;
}

void endRecordedOpsCaptureForCanvas(Canvas* canvas) {
  if (!canvas) {
    return;
  }
  if (auto* mc = dynamic_cast<MetalCanvas*>(canvas)) {
    mc->endRecordedOpsCapture();
  }
}

void replayRecordedOpsForCanvas(Canvas* canvas, MetalFrameRecorder const& recorded, MetalRecorderSlice const& slice) {
  if (!canvas) {
    return;
  }
  if (auto* mc = dynamic_cast<MetalCanvas*>(canvas)) {
    mc->replayRecordedOps(recorded, slice);
  }
}

bool requestNextFrameCaptureForCanvas(Canvas* canvas) {
  if (!canvas) {
    return false;
  }
  if (auto* mc = dynamic_cast<MetalCanvas*>(canvas)) {
    mc->requestNextFrameCapture();
    return true;
  }
  return false;
}

bool takeCapturedFrameForCanvas(Canvas* canvas, std::vector<std::uint8_t>& out, std::uint32_t& width,
                                std::uint32_t& height) {
  if (!canvas) {
    return false;
  }
  if (auto* mc = dynamic_cast<MetalCanvas*>(canvas)) {
    return mc->takeCapturedFrame(out, width, height);
  }
  return false;
}

} // namespace flux
