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
    const CGSize ds = metal_.layer().drawableSize;
    CGFloat cs = metal_.layer().contentsScale;
    if (cs < 0.01) {
      cs = 1.0;
    }
    dpiScaleX_ = static_cast<float>(cs);
    dpiScaleY_ = static_cast<float>(cs);
    if (logicalW_ <= 0 || logicalH_ <= 0) {
      logicalW_ = static_cast<int>(std::lround(static_cast<double>(ds.width) / static_cast<double>(cs)));
      logicalH_ = static_cast<int>(std::lround(static_cast<double>(ds.height) / static_cast<double>(cs)));
    }
    if (inFrame_) {
      glyphAtlas_->prepareForFrameBegin();
    }
  }

  void clear(Color color) override { clearColor_ = color; }

  void present() override {
    if (!inFrame_ || !cmdBuf_ || !drawable_) {
      return;
    }

    CGSize drawableSize = metal_.layer().drawableSize;
    const float vw = static_cast<float>(drawableSize.width);
    const float vh = static_cast<float>(drawableSize.height);
    if (vw < 1.f || vh < 1.f) {
      [cmdBuf_ commit];
      cmdBuf_ = nil;
      drawable_ = nil;
      dispatch_semaphore_signal(frameSem_);
      inFrame_ = false;
      return;
    }

    metal_.uploadInstanceInstances(frame_.ops);
    metal_.uploadImageInstances(frame_.ops);
    metal_.uploadPathVertices(frame_.pathVerts);
    metal_.uploadGlyphVertices(frame_.glyphVerts);

    MTLRenderPassDescriptor* pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = drawable_.texture;
    pass.colorAttachments[0].loadAction = MTLLoadActionClear;
    pass.colorAttachments[0].clearColor =
        MTLClearColorMake(clearColor_.r, clearColor_.g, clearColor_.b, clearColor_.a);
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;

    id<MTLRenderCommandEncoder> enc = [cmdBuf_ renderCommandEncoderWithDescriptor:pass];
    MTLViewport vp = {0, 0, drawableSize.width, drawableSize.height, 0.0, 1.0};
    [enc setViewport:vp];

    MTLScissorRect sc = {0, 0, static_cast<NSUInteger>(drawableSize.width),
                         static_cast<NSUInteger>(drawableSize.height)};
    if (clipScissorValid_) {
      sc = clipScissor_;
    }
    [enc setScissorRect:sc];

    NSUInteger instSlot = 0;
    NSUInteger imageSlot = 0;
    id<MTLBuffer> pathBuf = metal_.pathVertexArenaBuffer();
    for (const MetalDrawOp& op : frame_.ops) {
      switch (op.kind) {
      case MetalDrawOp::Rect: {
        [enc setRenderPipelineState:metal_.rectPSO(op.blendMode)];
        [enc setVertexBuffer:metal_.quadBuffer() offset:0 atIndex:0];
        const NSUInteger off = instSlot * sizeof(MetalRectInstance);
        ++instSlot;
        [enc setVertexBuffer:metal_.instanceArenaBuffer() offset:off atIndex:1];
        [enc drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:kQuadStripCount
                instanceCount:1];
        break;
      }
      case MetalDrawOp::Line: {
        [enc setRenderPipelineState:metal_.linePSO(op.blendMode)];
        [enc setVertexBuffer:metal_.quadBuffer() offset:0 atIndex:0];
        const NSUInteger off = instSlot * sizeof(MetalRectInstance);
        ++instSlot;
        [enc setVertexBuffer:metal_.instanceArenaBuffer() offset:off atIndex:1];
        [enc drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:kQuadStripCount
                instanceCount:1];
        break;
      }
      case MetalDrawOp::Image: {
        if (!op.texture) {
          break;
        }
        [enc setRenderPipelineState:metal_.imagePSO(op.blendMode)];
        [enc setVertexBuffer:metal_.quadBuffer() offset:0 atIndex:0];
        const NSUInteger off = imageSlot * sizeof(MetalImageInstance);
        ++imageSlot;
        [enc setVertexBuffer:metal_.imageInstanceArenaBuffer() offset:off atIndex:1];
        [enc setFragmentTexture:(__bridge id<MTLTexture>)op.texture atIndex:0];
        [enc setFragmentSamplerState:op.repeatSampler ? metal_.repeatSampler() : metal_.linearSampler() atIndex:0];
        [enc drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:kQuadStripCount
                instanceCount:1];
        break;
      }
      case MetalDrawOp::PathMesh: {
        if (op.pathCount == 0) {
          break;
        }
        [enc setRenderPipelineState:metal_.pathPSO(op.blendMode)];
        const NSUInteger off = static_cast<NSUInteger>(op.pathStart) * sizeof(PathVertex);
        [enc setVertexBuffer:pathBuf offset:off atIndex:0];
        [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:static_cast<NSUInteger>(op.pathCount)];
        break;
      }
      case MetalDrawOp::GlyphMesh: {
        if (op.glyphVertexCount == 0) {
          break;
        }
        [enc setRenderPipelineState:metal_.glyphPSO(op.blendMode)];
        id<MTLBuffer> gbuf = metal_.glyphVertexArenaBuffer();
        const NSUInteger goff = static_cast<NSUInteger>(op.glyphStart) * sizeof(MetalGlyphVertex);
        [enc setVertexBuffer:gbuf offset:goff atIndex:0];
        float vp[2] = {vw, vh};
        [enc setVertexBytes:vp length:sizeof(vp) atIndex:1];
        [enc setFragmentTexture:glyphAtlas_->texture() atIndex:0];
        [enc setFragmentSamplerState:metal_.linearSampler() atIndex:0];
        [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:static_cast<NSUInteger>(op.glyphVertexCount)];
        break;
      }
      }
    }

    [enc endEncoding];

    __block dispatch_semaphore_t sem = frameSem_;
    [cmdBuf_ addCompletedHandler:^(id<MTLCommandBuffer> /*cb*/) { dispatch_semaphore_signal(sem); }];
    [cmdBuf_ presentDrawable:drawable_];
    [cmdBuf_ commit];

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
    currentState().transform = currentState().transform * m;
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
                StrokeStyle const& ss) override {
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
    if (!hasFill && !hasStroke) {
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

    float rotationRad = 0.f;
    float tx = 0.f;
    float ty = 0.f;
    bool const decomposed = tryDecomposeRotationTranslation(M, &rotationRad, &tx, &ty);
    Rect const mapped = boundsOfTransformedRect(drawR, M);
    if (decomposed) {
      rotationRad = 0.f;
    }

    const float s = std::min(dpiScaleX_, dpiScaleY_);
    CornerRadius cr{};
    cr.topLeft = cornerRadius.topLeft * s;
    cr.topRight = cornerRadius.topRight * s;
    cr.bottomRight = cornerRadius.bottomRight * s;
    cr.bottomLeft = cornerRadius.bottomLeft * s;
    Rect device = Rect::sharp(mapped.x * dpiScaleX_, mapped.y * dpiScaleY_, mapped.width * dpiScaleX_,
                              mapped.height * dpiScaleY_);
    emitRect(device, cr, hasFill ? fillC : Color{0, 0, 0, 0}, hasStroke ? strokeC : Color{0, 0, 0, 0},
             hasStroke ? ss.width * s : 0.f, op, rotationRad);
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
    Point ta = currentState().transform.apply(a);
    Point tb = currentState().transform.apply(b);
    const float dx = tb.x - ta.x;
    const float dy = tb.y - ta.y;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-4f) {
      return;
    }
    const float w = len + pad * 2.f;
    const float h = ss.width + pad * 2.f;
    const float cx = (ta.x + tb.x) * 0.5f;
    const float cy = (ta.y + tb.y) * 0.5f;
    Rect const lineBounds = Rect::sharp(cx - w * 0.5f, cy - h * 0.5f, w, h);

    CGSize drawableSize = metal_.layer().drawableSize;
    const float vw = static_cast<float>(drawableSize.width);
    const float vh = static_cast<float>(drawableSize.height);

    MetalDrawOp op{};
    op.kind = MetalDrawOp::Line;
    op.rectInst.rect = simd_make_float4(lineBounds.x * dpiScaleX_, lineBounds.y * dpiScaleY_,
                                        lineBounds.width * dpiScaleX_, lineBounds.height * dpiScaleY_);
    const float inv = 1.f / len;
    const float lenDevice = std::hypot(dx * dpiScaleX_, dy * dpiScaleY_);
    op.rectInst.corners = simd_make_float4(dx * inv, dy * inv, lenDevice * 0.5f, 0.f);
    op.rectInst.fillColor = simd_make_float4(0.f, 0.f, 0.f, 0.f);
    op.rectInst.strokeColor = toSimd4(stroke);
    op.rectInst.strokeWidthOpacity =
        simd_make_float2(ss.width * std::min(dpiScaleX_, dpiScaleY_), paintOpacity);
    op.rectInst.viewport = simd_make_float2(vw, vh);
    op.rectInst.rotationPad = simd_make_float4(0.f, 0.f, 0.f, 0.f);
    op.blendMode = currentState().blendMode;
    frame_.ops.push_back(op);
  }

  void drawPath(Path const& path, FillStyle const& fs, StrokeStyle const& ss) override {
    if (!inFrame_ || path.isEmpty()) {
      return;
    }

    if (path.commandCount() == 1) {
      Path::CommandView cv = path.command(0);
      if (cv.type == Path::CommandType::Rect && cv.dataCount >= 8) {
        const float* d = cv.data;
        Rect r{d[0], d[1], d[2], d[3]};
        CornerRadius cr{d[4], d[5], d[6], d[7]};
        drawRect(r, cr, fs, ss);
        return;
      }
      const bool circlePrim = cv.type == Path::CommandType::Circle && cv.dataCount >= 3;
      const bool ellipsePrim = cv.type == Path::CommandType::Ellipse && cv.dataCount >= 4;
      if (circlePrim || ellipsePrim) {
        Color fc{};
        if (!fs.isNone() && fs.solidColor(&fc)) {
          if (circlePrim) {
            drawCircle({cv.data[0], cv.data[1]}, cv.data[2], fs, ss);
          } else {
            drawCircle({cv.data[0], cv.data[1]}, std::max(cv.data[2], cv.data[3]), fs, ss);
          }
          return;
        }
      }
    }

    CGSize drawableSize = metal_.layer().drawableSize;
    const float vw = static_cast<float>(drawableSize.width);
    const float vh = static_cast<float>(drawableSize.height);

    metalPathRasterizeToMesh(path, fs, ss, currentState().transform, dpiScaleX_, dpiScaleY_, effectiveOpacity(), vw,
                             vh, frame_.pathVerts, frame_.ops, currentState().blendMode);
  }

  void drawCircle(Point center, float radius, FillStyle const& fs, StrokeStyle const& ss) override {
    Rect r{center.x - radius, center.y - radius, radius * 2.f, radius * 2.f};
    drawRect(r, CornerRadius::pill(r), fs, ss);
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

    float rotationRad = 0.f;
    float tx = 0.f;
    float ty = 0.f;
    Rect mapped{};
    if (tryDecomposeRotationTranslation(M, &rotationRad, &tx, &ty)) {
      mapped = Rect::sharp(dst.x + tx, dst.y + ty, dst.width, dst.height);
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

    const float s = std::min(dpiScaleX_, dpiScaleY_);
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

    float rotationRad = 0.f;
    float tx = 0.f;
    float ty = 0.f;
    Rect mapped{};
    if (tryDecomposeRotationTranslation(M, &rotationRad, &tx, &ty)) {
      mapped = Rect::sharp(dst.x + tx, dst.y + ty, dst.width, dst.height);
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

    const float s = std::min(dpiScaleX_, dpiScaleY_);
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

    Mat3 const& M = currentState().transform;
    BlendMode const blend = currentState().blendMode;
    float const op = effectiveOpacity();

    float const aw = static_cast<float>(glyphAtlas_->atlasPixelWidth());
    float const ah = static_cast<float>(glyphAtlas_->atlasPixelHeight());
    if (aw < 1.f || ah < 1.f) {
      return;
    }

    std::uint32_t const glyphStart = static_cast<std::uint32_t>(frame_.glyphVerts.size());
    for (auto const& placed : layout.runs) {
      TextRun const& text = placed.run;
      if (text.glyphIds.empty()) {
        continue;
      }

      float const baselineY = origin.y + placed.origin.y;
      float const x = origin.x + placed.origin.x;

      float const effectiveAlpha = text.color.a * op;
      vector_float4 const premul = simd_make_float4(text.color.r * effectiveAlpha,
                                                    text.color.g * effectiveAlpha,
                                                    text.color.b * effectiveAlpha,
                                                    effectiveAlpha);

      float const physicalFontSize = text.fontSize * dpiScaleX_;

      for (std::size_t i = 0; i < text.glyphIds.size(); ++i) {
        GlyphKey key{};
        key.fontId = text.fontId;
        key.glyphId = text.glyphIds[i];
        unsigned const q = static_cast<unsigned>(physicalFontSize * 4.f);
        key.sizeQ8 = static_cast<std::uint16_t>(std::min(65535u, q));

        AtlasEntry const& entry = glyphAtlas_->getOrUpload(key);
        if (entry.width == 0 || entry.height == 0) {
          continue;
        }

        float const u0 = static_cast<float>(entry.u) / aw;
        float const u1 = static_cast<float>(entry.u + entry.width) / aw;
        float const vLo = static_cast<float>(entry.v) / ah;
        float const vHi = static_cast<float>(entry.v + entry.height) / ah;

        Point const ink = {x + text.positions[i].x, baselineY + text.positions[i].y};
        Point const tl = {ink.x - entry.bearing.x / dpiScaleX_, ink.y - entry.bearing.y / dpiScaleY_};
        float const gw = static_cast<float>(entry.width) / dpiScaleX_;
        float const gh = static_cast<float>(entry.height) / dpiScaleY_;

        appendGlyphQuad(frame_.glyphVerts, M, dpiScaleX_, dpiScaleY_, tl, gw, gh, u0, vLo, u1, vHi, premul);
      }
    }

    std::uint32_t const vertCount =
        static_cast<std::uint32_t>(frame_.glyphVerts.size()) - glyphStart;
    if (vertCount > 0) {
      MetalDrawOp op{};
      op.kind = MetalDrawOp::GlyphMesh;
      op.glyphStart = glyphStart;
      op.glyphVertexCount = vertCount;
      op.blendMode = blend;
      frame_.ops.push_back(op);
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
  id<CAMetalDrawable> drawable_{nil};
  bool inFrame_{false};

  Color clearColor_{0.f, 0.f, 0.f, 1.f};
  int logicalW_{0};
  int logicalH_{0};
  float dpiScaleX_{1.f};
  float dpiScaleY_{1.f};

  MetalFrameRecorder frame_;
  std::vector<GpuState> stateStack_;

  MTLScissorRect clipScissor_{};
  bool clipScissorValid_{false};

  GpuState& currentState() { return stateStack_.back(); }
  GpuState const& currentState() const { return stateStack_.back(); }

  void pushState() { stateStack_.push_back(stateStack_.empty() ? GpuState{} : stateStack_.back()); }

  void popState() {
    stateStack_.pop_back();
    updateClipScissor();
  }

  float effectiveOpacity() const { return currentState().opacity; }

  Rect viewportLogicalRect() const {
    if (logicalW_ > 0 && logicalH_ > 0) {
      return Rect::sharp(0, 0, static_cast<float>(logicalW_), static_cast<float>(logicalH_));
    }
    CGSize ds = metal_.layer().drawableSize;
    return Rect::sharp(0, 0, static_cast<float>(ds.width) / dpiScaleX_, static_cast<float>(ds.height) / dpiScaleY_);
  }

  void updateClipScissor() {
    if (!currentState().clip.has_value()) {
      clipScissorValid_ = false;
      return;
    }
    Rect c = *currentState().clip;
    float x0 = c.x * dpiScaleX_;
    float y0 = c.y * dpiScaleY_;
    float x1 = (c.x + c.width) * dpiScaleX_;
    float y1 = (c.y + c.height) * dpiScaleY_;
    float minX = std::min(x0, x1);
    float minY = std::min(y0, y1);
    float maxX = std::max(x0, x1);
    float maxY = std::max(y0, y1);
    CGSize drawableSize = metal_.layer().drawableSize;
    NSUInteger dw = static_cast<NSUInteger>(drawableSize.width);
    NSUInteger dh = static_cast<NSUInteger>(drawableSize.height);
    clipScissor_.x = static_cast<NSUInteger>(std::clamp(minX, 0.f, static_cast<float>(dw - 1)));
    clipScissor_.y = static_cast<NSUInteger>(std::clamp(minY, 0.f, static_cast<float>(dh - 1)));
    clipScissor_.width = static_cast<NSUInteger>(std::clamp(maxX - minX, 0.f, static_cast<float>(dw)));
    clipScissor_.height = static_cast<NSUInteger>(std::clamp(maxY - minY, 0.f, static_cast<float>(dh)));
    clipScissorValid_ = clipScissor_.width > 0 && clipScissor_.height > 0;
  }

  void emitRect(Rect const& deviceRect, CornerRadius const& corners, Color const& fillColor, Color const& strokeColor,
                float strokeWidth, float opacity, float rotationRad) {
    CGSize drawableSize = metal_.layer().drawableSize;
    const float vw = static_cast<float>(drawableSize.width);
    const float vh = static_cast<float>(drawableSize.height);

    MetalDrawOp op{};
    op.kind = MetalDrawOp::Rect;
    op.rectInst.rect = simd_make_float4(deviceRect.x, deviceRect.y, deviceRect.width, deviceRect.height);
    op.rectInst.corners = cornersToSimd(corners);
    op.rectInst.fillColor = toSimd4(fillColor);
    op.rectInst.strokeColor = toSimd4(strokeColor);
    op.rectInst.strokeWidthOpacity = simd_make_float2(strokeWidth, opacity);
    op.rectInst.viewport = simd_make_float2(vw, vh);
    op.rectInst.rotationPad = simd_make_float4(rotationRad, 0.f, 0.f, 0.f);
    op.blendMode = currentState().blendMode;
    frame_.ops.push_back(op);
  }

  void emitImage(id<MTLTexture> tex, Rect const& deviceRect, CornerRadius const& corners, vector_float4 const& uvBounds,
                 vector_float2 const& texSizeInv, float imageMode, float opacity, float rotationRad, bool repeat) {
    CGSize drawableSize = metal_.layer().drawableSize;
    const float vw = static_cast<float>(drawableSize.width);
    const float vh = static_cast<float>(drawableSize.height);

    MetalDrawOp op{};
    op.kind = MetalDrawOp::Image;
    op.imageInst.sdf.rect = simd_make_float4(deviceRect.x, deviceRect.y, deviceRect.width, deviceRect.height);
    op.imageInst.sdf.corners = cornersToSimd(corners);
    op.imageInst.sdf.fillColor = simd_make_float4(1.f, 1.f, 1.f, 1.f);
    op.imageInst.sdf.strokeColor = simd_make_float4(0.f, 0.f, 0.f, 0.f);
    op.imageInst.sdf.strokeWidthOpacity = simd_make_float2(0.f, opacity);
    op.imageInst.sdf.viewport = simd_make_float2(vw, vh);
    op.imageInst.sdf.rotationPad = simd_make_float4(rotationRad, 0.f, 0.f, 0.f);
    op.imageInst.uvBounds = uvBounds;
    op.imageInst.texSizeInv = texSizeInv;
    op.imageInst.imageModePad = simd_make_float2(imageMode, 0.f);
    op.blendMode = currentState().blendMode;
    op.texture = (__bridge_retained void*)tex;
    op.repeatSampler = repeat;
    frame_.ops.push_back(op);
  }
};

std::unique_ptr<Canvas> createMetalCanvas(Window* window, void* caMetalLayer, unsigned int handle,
                                          TextSystem& textSystem) {
  return std::make_unique<MetalCanvas>(window, (__bridge CAMetalLayer*)caMetalLayer, handle, textSystem);
}

} // namespace flux
