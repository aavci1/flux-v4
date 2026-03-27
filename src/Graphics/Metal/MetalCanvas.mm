#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>
#import <simd/simd.h>

#include <Flux/Graphics/Canvas.hpp>

#include "Graphics/Metal/MetalCanvasTypes.hpp"
#include "Graphics/Metal/MetalDeviceResources.hpp"
#include "Graphics/Metal/MetalFrameRecorder.hpp"
#include "Graphics/Metal/MetalPathRasterizer.hpp"

namespace flux {
class Window;
}

#include "Graphics/Metal/MetalCanvas.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
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

} // namespace

class MetalCanvas final : public Canvas {
public:
  MetalCanvas(Window* /*window*/, CAMetalLayer* layer, unsigned int handle)
      : metal_(layer), windowHandle_(handle) {
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
    dispatch_semaphore_wait(frameSem_, DISPATCH_TIME_FOREVER);
    drawable_ = [metal_.layer() nextDrawable];
    cmdBuf_ = [metal_.queue() commandBuffer];
    if (!drawable_) {
      dispatch_semaphore_signal(frameSem_);
      cmdBuf_ = nil;
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
    metal_.uploadPathVertices(frame_.pathVerts);

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
      }
    }

    [enc endEncoding];

    __block dispatch_semaphore_t sem = frameSem_;
    [cmdBuf_ addCompletedHandler:^(id<MTLCommandBuffer> /*cb*/) { dispatch_semaphore_signal(sem); }];
    [cmdBuf_ presentDrawable:drawable_];
    [cmdBuf_ commit];

    frame_.clear();

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
    Rect mapped = boundsOfTransformedRect(rect, currentState().transform);
    return !intersects(mapped, *currentState().clip);
  }

  void setOpacity(float o) override { currentState().opacity = std::clamp(o, 0.f, 1.f); }

  float opacity() const override { return currentState().opacity; }

  void setBlendMode(BlendMode mode) override { currentState().blendMode = mode; }

  BlendMode blendMode() const override { return currentState().blendMode; }

  void setFillStyle(FillStyle const& style) override { currentState().fillStyle = style; }

  FillStyle fillStyle() const override { return currentState().fillStyle; }

  void setStrokeStyle(StrokeStyle const& style) override { currentState().strokeStyle = style; }

  StrokeStyle strokeStyle() const override { return currentState().strokeStyle; }

  void drawRect(Rect const& rect, CornerRadius const& cornerRadius) override {
    if (!inFrame_) {
      return;
    }
    FillStyle const& fs = currentState().fillStyle;
    StrokeStyle const& ss = currentState().strokeStyle;
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
    Rect const mappedClip = boundsOfTransformedRect(rect, M);
    if (currentState().clip.has_value()) {
      Rect inter = intersectRects(mappedClip, *currentState().clip);
      if (inter.width <= 0.f || inter.height <= 0.f) {
        return;
      }
    }

    float rotationRad = 0.f;
    float tx = 0.f;
    float ty = 0.f;
    Rect mapped{};
    if (tryDecomposeRotationTranslation(M, &rotationRad, &tx, &ty)) {
      mapped = Rect::sharp(rect.x + tx, rect.y + ty, rect.width, rect.height);
    } else {
      mapped = mappedClip;
      if (currentState().clip.has_value()) {
        mapped = intersectRects(mapped, *currentState().clip);
        if (mapped.width <= 0.f || mapped.height <= 0.f) {
          return;
        }
      }
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

  void drawLine(Point a, Point b) override {
    if (!inFrame_) {
      return;
    }
    StrokeStyle const& ss = currentState().strokeStyle;
    Color stroke{};
    if (!ss.solidColor(&stroke)) {
      return;
    }
    Point ta = currentState().transform.apply(a);
    Point tb = currentState().transform.apply(b);
    const float dx = tb.x - ta.x;
    const float dy = tb.y - ta.y;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-4f) {
      return;
    }
    const float paintOpacity = effectiveOpacity();
    const float pad = std::max(ss.width * 2.f, 4.f);
    const float w = len + pad * 2.f;
    const float h = ss.width + pad * 2.f;
    const float cx = (ta.x + tb.x) * 0.5f;
    const float cy = (ta.y + tb.y) * 0.5f;
    Rect lineBounds = Rect::sharp(cx - w * 0.5f, cy - h * 0.5f, w, h);
    if (currentState().clip.has_value() && !intersects(lineBounds, *currentState().clip)) {
      return;
    }

    CGSize drawableSize = metal_.layer().drawableSize;
    const float vw = static_cast<float>(drawableSize.width);
    const float vh = static_cast<float>(drawableSize.height);

    MetalDrawOp op;
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

  void drawPath(Path const& path) override {
    if (!inFrame_ || path.isEmpty()) {
      return;
    }

    FillStyle const& fs = currentState().fillStyle;
    StrokeStyle const& ss = currentState().strokeStyle;

    if (path.commandCount() == 1) {
      Path::CommandView cv = path.command(0);
      if (cv.type == Path::CommandType::Rect && cv.dataCount >= 8) {
        const float* d = cv.data;
        Rect r{d[0], d[1], d[2], d[3], CornerRadius{d[4], d[5], d[6], d[7]}};
        drawRect(r, r.corners);
        return;
      }
      const bool circlePrim = cv.type == Path::CommandType::Circle && cv.dataCount >= 3;
      const bool ellipsePrim = cv.type == Path::CommandType::Ellipse && cv.dataCount >= 4;
      if (circlePrim || ellipsePrim) {
        Color fc{};
        if (!fs.isNone() && fs.solidColor(&fc)) {
          if (circlePrim) {
            drawCircle({cv.data[0], cv.data[1]}, cv.data[2]);
          } else {
            drawCircle({cv.data[0], cv.data[1]}, std::max(cv.data[2], cv.data[3]));
          }
          return;
        }
      }
    }

    CGSize drawableSize = metal_.layer().drawableSize;
    const float vw = static_cast<float>(drawableSize.width);
    const float vh = static_cast<float>(drawableSize.height);

    metalPathRasterizeToMesh(path, fs, ss, currentState().transform, dpiScaleX_, dpiScaleY_, effectiveOpacity(),
                             vw, vh, frame_.pathVerts, frame_.ops, currentState().blendMode);
  }

  void drawCircle(Point center, float radius) override {
    Rect r = Rect::pill(center.x - radius, center.y - radius, radius * 2.f, radius * 2.f);
    drawRect(r, r.corners);
  }

private:
  struct GpuState {
    Mat3 transform = Mat3::identity();
    float opacity = 1.f;
    BlendMode blendMode = BlendMode::Normal;
    std::optional<Rect> clip;
    FillStyle fillStyle = FillStyle::solid(Colors::black);
    StrokeStyle strokeStyle = StrokeStyle::none();
  };

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

    MetalDrawOp op;
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
};

std::unique_ptr<Canvas> createMetalCanvas(Window* window, void* caMetalLayer, unsigned int handle) {
  return std::make_unique<MetalCanvas>(window, (__bridge CAMetalLayer*)caMetalLayer, handle);
}

} // namespace flux
