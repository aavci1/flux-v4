#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>
#import <simd/simd.h>

#include <Flux/Graphics/Canvas.hpp>

#include "Graphics/PathFlattener.hpp"

namespace flux {
class Window;
}

#include "Graphics/Metal/MetalCanvas.hpp"

namespace flux::detail {
extern const char kFluxShaderMSL[];
}

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

namespace flux {

namespace {

constexpr NSUInteger kQuadStripCount = 4;
constexpr NSUInteger kFramesInFlight = 3;

struct RectInstance {
  vector_float4 rect;
  vector_float4 corners;
  vector_float4 fillColor;
  vector_float4 strokeColor;
  vector_float2 strokeWidthOpacity;
  vector_float2 viewport;
  vector_float4 rotationPad;
};

struct DrawOp {
  enum Kind : std::uint8_t { Rect, Line, PathMesh } kind = Rect;
  RectInstance rectInst{};
  std::uint32_t pathStart = 0;
  std::uint32_t pathCount = 0;
};

vector_float4 toSimd4(const Color& c) { return simd_make_float4(c.r, c.g, c.b, c.a); }

vector_float4 cornersToSimd(const CornerRadius& cr) {
  return simd_make_float4(cr.topLeft, cr.topRight, cr.bottomRight, cr.bottomLeft);
}

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
    NSLog(@"Flux MetalCanvas: pipeline error: %@", err);
  }
  return pso;
}

id<MTLRenderPipelineState> makePathPipeline(id<MTLDevice> device, id<MTLLibrary> lib, MTLPixelFormat colorFormat, bool blend) {
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
    NSLog(@"Flux MetalCanvas: path pipeline error: %@", err);
  }
  return pso;
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

/// If `m` is rotation × translation only (orthogonal det≈1, scale≈1), we can draw the unrotated rect + pass θ to the shader.
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
      : layer_(layer), windowHandle_(handle) {
    device_ = layer_.device ? layer_.device : MTLCreateSystemDefaultDevice();
    layer_.device = device_;
    layer_.pixelFormat = MTLPixelFormatBGRA8Unorm;
    queue_ = [device_ newCommandQueue];
    frameSem_ = dispatch_semaphore_create(static_cast<int>(kFramesInFlight));

    NSError* err = nil;
    NSString* src = [NSString stringWithUTF8String:flux::detail::kFluxShaderMSL];
    MTLCompileOptions* opts = [[MTLCompileOptions alloc] init];
    opts.languageVersion = MTLLanguageVersion2_4;
    id<MTLLibrary> lib = [device_ newLibraryWithSource:src options:opts error:&err];
    if (!lib) {
      NSLog(@"Flux MetalCanvas: failed to compile embedded Metal library: %@", err);
      throw std::runtime_error("MetalCanvas: could not compile embedded shaders");
    }
    lib_ = lib;

    MTLPixelFormat pf = layer_.pixelFormat;
    rectPSO_ = makePipeline(device_, lib_, @"rect_sdf_vert", @"rect_sdf_frag", pf, true);
    linePSO_ = makePipeline(device_, lib_, @"line_sdf_vert", @"line_sdf_frag", pf, true);
    pathPSO_ = makePathPipeline(device_, lib_, pf, true);
    if (!rectPSO_ || !linePSO_ || !pathPSO_) {
      throw std::runtime_error("MetalCanvas: pipeline creation failed");
    }

    static const vector_float2 kQuadStrip[kQuadStripCount] = {
        {-1.f, -1.f},
        {1.f, -1.f},
        {-1.f, 1.f},
        {1.f, 1.f},
    };
    quadBuffer_ = [device_ newBufferWithBytes:kQuadStrip
                                       length:sizeof(kQuadStrip)
                                      options:MTLResourceStorageModeShared];

    pushState();
  }

  ~MetalCanvas() override { ops_.clear(); }

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
    ops_.clear();
    pathVerts_.clear();
    dispatch_semaphore_wait(frameSem_, DISPATCH_TIME_FOREVER);
    drawable_ = [layer_ nextDrawable];
    cmdBuf_ = [queue_ commandBuffer];
    if (!drawable_) {
      dispatch_semaphore_signal(frameSem_);
      cmdBuf_ = nil;
    }
    inFrame_ = (drawable_ != nil && cmdBuf_ != nil);
    const CGSize ds = layer_.drawableSize;
    CGFloat cs = layer_.contentsScale;
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

    CGSize drawableSize = layer_.drawableSize;
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

    id<MTLBuffer> pathBuf = nil;
    for (const DrawOp& op : ops_) {
      switch (op.kind) {
      case DrawOp::Rect: {
        [enc setRenderPipelineState:rectPSO_];
        [enc setVertexBuffer:quadBuffer_ offset:0 atIndex:0];
        id<MTLBuffer> inst = [device_ newBufferWithBytes:&op.rectInst length:sizeof(RectInstance)
                                                   options:MTLResourceStorageModeShared];
        [enc setVertexBuffer:inst offset:0 atIndex:1];
        [enc drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:kQuadStripCount
                instanceCount:1];
        break;
      }
      case DrawOp::Line: {
        [enc setRenderPipelineState:linePSO_];
        [enc setVertexBuffer:quadBuffer_ offset:0 atIndex:0];
        id<MTLBuffer> inst = [device_ newBufferWithBytes:&op.rectInst length:sizeof(RectInstance)
                                                   options:MTLResourceStorageModeShared];
        [enc setVertexBuffer:inst offset:0 atIndex:1];
        [enc drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:kQuadStripCount
                instanceCount:1];
        break;
      }
      case DrawOp::PathMesh: {
        if (op.pathCount == 0) {
          break;
        }
        if (!pathBuf) {
          pathBuf = [device_ newBufferWithBytes:pathVerts_.data()
                                           length:pathVerts_.size() * sizeof(PathVertex)
                                          options:MTLResourceStorageModeShared];
        }
        [enc setRenderPipelineState:pathPSO_];
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

    ops_.clear();

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

    CGSize drawableSize = layer_.drawableSize;
    const float vw = static_cast<float>(drawableSize.width);
    const float vh = static_cast<float>(drawableSize.height);

    DrawOp op;
    op.kind = DrawOp::Line;
    op.rectInst.rect = simd_make_float4(lineBounds.x * dpiScaleX_, lineBounds.y * dpiScaleY_,
                                        lineBounds.width * dpiScaleX_, lineBounds.height * dpiScaleY_);
    const float inv = 1.f / len;
    const float lenDevice = std::hypot(dx * dpiScaleX_, dy * dpiScaleY_);
    // Segment half-length in device pixels (must not include quad padding; see line_sdf_frag).
    op.rectInst.corners = simd_make_float4(dx * inv, dy * inv, lenDevice * 0.5f, 0.f);
    op.rectInst.fillColor = simd_make_float4(0.f, 0.f, 0.f, 0.f);
    op.rectInst.strokeColor = toSimd4(stroke);
    op.rectInst.strokeWidthOpacity =
        simd_make_float2(ss.width * std::min(dpiScaleX_, dpiScaleY_), paintOpacity);
    op.rectInst.viewport = simd_make_float2(vw, vh);
    op.rectInst.rotationPad = simd_make_float4(0.f, 0.f, 0.f, 0.f);
    ops_.push_back(op);
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

    CGSize drawableSize = layer_.drawableSize;
    const float vw = static_cast<float>(drawableSize.width);
    const float vh = static_cast<float>(drawableSize.height);
    if (vw < 1.f || vh < 1.f) {
      return;
    }

    const float s = std::min(dpiScaleX_, dpiScaleY_);
    Mat3 const& M = currentState().transform;
    const float op = effectiveOpacity();
    const size_t pathBegin = pathVerts_.size();

    auto subpaths = PathFlattener::flattenSubpaths(path);
    for (auto& sp : subpaths) {
      for (auto& p : sp) {
        Point q = M.apply(p);
        p = {q.x * dpiScaleX_, q.y * dpiScaleY_};
      }
    }

    auto appendVerts = [this](TessellatedPath&& t) {
      if (t.vertices.empty()) {
        return;
      }
      pathVerts_.insert(pathVerts_.end(), t.vertices.begin(), t.vertices.end());
    };

    if (!fs.isNone()) {
      Color fc{};
      if (fs.solidColor(&fc)) {
        fc.a *= op;
        if (subpaths.size() > 1) {
          std::vector<std::vector<Point>> nonempty;
          nonempty.reserve(subpaths.size());
          for (const auto& s : subpaths) {
            if (s.size() >= 3) {
              nonempty.push_back(s);
            }
          }
          if (!nonempty.empty()) {
            appendVerts(PathFlattener::tessellateFillContours(nonempty, fc, vw, vh,
                                                              PathFlattener::tessWindingFromFillRule(fs.fillRule)));
          }
        } else {
          for (auto& s : subpaths) {
            if (s.size() >= 3) {
              appendVerts(PathFlattener::tessellateFill(s, fc, vw, vh));
            }
          }
        }
      }
    }

    if (!ss.isNone()) {
      Color sc{};
      if (ss.solidColor(&sc)) {
        sc.a *= op;
        const float sw = ss.width * s;
        for (const auto& sp : subpaths) {
          if (sp.size() >= 2) {
            appendVerts(PathFlattener::tessellateStroke(sp, sw, sc, vw, vh));
          }
        }
      }
    }

    const size_t pathEnd = pathVerts_.size();
    if (pathEnd > pathBegin) {
      DrawOp pop{};
      pop.kind = DrawOp::PathMesh;
      pop.pathStart = static_cast<std::uint32_t>(pathBegin);
      pop.pathCount = static_cast<std::uint32_t>(pathEnd - pathBegin);
      ops_.push_back(pop);
    }
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

  CAMetalLayer* layer_{nil};
  unsigned int windowHandle_{0};
  id<MTLDevice> device_{nil};
  id<MTLCommandQueue> queue_{nil};
  id<MTLLibrary> lib_{nil};
  id<MTLRenderPipelineState> rectPSO_{nil};
  id<MTLRenderPipelineState> linePSO_{nil};
  id<MTLRenderPipelineState> pathPSO_{nil};
  id<MTLBuffer> quadBuffer_{nil};

  dispatch_semaphore_t frameSem_{nullptr};
  id<MTLCommandBuffer> cmdBuf_{nil};
  id<CAMetalDrawable> drawable_{nil};
  bool inFrame_{false};

  Color clearColor_{0.f, 0.f, 0.f, 1.f};
  int logicalW_{0};
  int logicalH_{0};
  float dpiScaleX_{1.f};
  float dpiScaleY_{1.f};

  std::vector<DrawOp> ops_;
  std::vector<PathVertex> pathVerts_;
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
    CGSize ds = layer_.drawableSize;
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
    CGSize drawableSize = layer_.drawableSize;
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
    CGSize drawableSize = layer_.drawableSize;
    const float vw = static_cast<float>(drawableSize.width);
    const float vh = static_cast<float>(drawableSize.height);

    DrawOp op;
    op.kind = DrawOp::Rect;
    op.rectInst.rect = simd_make_float4(deviceRect.x, deviceRect.y, deviceRect.width, deviceRect.height);
    op.rectInst.corners = cornersToSimd(corners);
    op.rectInst.fillColor = toSimd4(fillColor);
    op.rectInst.strokeColor = toSimd4(strokeColor);
    op.rectInst.strokeWidthOpacity = simd_make_float2(strokeWidth, opacity);
    op.rectInst.viewport = simd_make_float2(vw, vh);
    op.rectInst.rotationPad = simd_make_float4(rotationRad, 0.f, 0.f, 0.f);
    ops_.push_back(op);
  }
};

std::unique_ptr<Canvas> createMetalCanvas(Window* window, void* caMetalLayer, unsigned int handle) {
  return std::make_unique<MetalCanvas>(window, (__bridge CAMetalLayer*)caMetalLayer, handle);
}

} // namespace flux
