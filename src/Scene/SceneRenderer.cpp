#include <Flux/Scene/SceneRenderer.hpp>

#include <Flux/Graphics/Canvas.hpp>
#include <Flux/Scene/Nodes.hpp>
#include <Flux/Scene/SceneGraph.hpp>

#include "Graphics/Metal/MetalCanvas.hpp"
#include "Graphics/Metal/MetalFrameRecorder.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace flux {

namespace {

struct RenderChildLayerCmd {
  NodeId child{};
};

struct ReplayOpBatch {
  MetalRecorderSlice slice{};
};

using LayerReplayStep = std::variant<ReplayOpBatch, RenderChildLayerCmd>;

struct CanvasStateSnapshot {
  Mat3 transform = Mat3::identity();
  Rect clip = Rect::sharp(0.f, 0.f, 0.f, 0.f);
  float opacity = 1.f;
  BlendMode blendMode = BlendMode::Normal;
};

bool mat3Equal(Mat3 const& a, Mat3 const& b) {
  for (int i = 0; i < 9; ++i) {
    if (a.m[i] != b.m[i]) {
      return false;
    }
  }
  return true;
}

CanvasStateSnapshot captureCanvasState(Canvas const& canvas) {
  return CanvasStateSnapshot{
      .transform = canvas.currentTransform(),
      .clip = canvas.clipBounds(),
      .opacity = canvas.opacity(),
      .blendMode = canvas.blendMode(),
  };
}

bool canvasStateEqual(CanvasStateSnapshot const& lhs, CanvasStateSnapshot const& rhs) {
  return mat3Equal(lhs.transform, rhs.transform) && lhs.clip == rhs.clip && lhs.opacity == rhs.opacity &&
         lhs.blendMode == rhs.blendMode;
}

class MetalRecordingCanvas final : public Canvas {
public:
  explicit MetalRecordingCanvas(Canvas& target)
      : target_(target) {
    stateStack_.push_back(State{
        .transform = target.currentTransform(),
        .clip = target.clipBounds(),
        .opacity = target.opacity(),
        .blendMode = target.blendMode(),
    });
  }

  Backend backend() const noexcept override { return target_.backend(); }
  unsigned int windowHandle() const override { return target_.windowHandle(); }
  void resize(int width, int height) override { target_.resize(width, height); }
  void updateDpiScale(float scaleX, float scaleY) override { target_.updateDpiScale(scaleX, scaleY); }
  void beginFrame() override { target_.beginFrame(); }
  void present() override { target_.present(); }

  void save() override {
    stateStack_.push_back(currentState());
    target_.save();
  }

  void restore() override {
    if (stateStack_.size() > 1) {
      stateStack_.pop_back();
    }
    target_.restore();
  }

  void setTransform(Mat3 const& m) override {
    currentState().transform = m;
    target_.setTransform(m);
  }

  void transform(Mat3 const& m) override {
    currentState().transform = currentState().transform * m;
    target_.transform(m);
  }

  void translate(Point offset) override { translate(offset.x, offset.y); }
  void translate(float x, float y) override { transform(Mat3::translate(x, y)); }
  void scale(float sx, float sy) override { transform(Mat3::scale(sx, sy)); }
  void scale(float s) override { scale(s, s); }
  void rotate(float radians) override { transform(Mat3::rotate(radians)); }
  void rotate(float radians, Point pivot) override { transform(Mat3::rotate(radians, pivot)); }

  Mat3 currentTransform() const override { return currentState().transform; }

  void clipRect(Rect rect, bool antiAlias = false) override {
    std::optional<Rect>& clip = currentState().clip;
    if (!clip.has_value()) {
      clip = rect;
    } else {
      float const x0 = std::max(clip->x, rect.x);
      float const y0 = std::max(clip->y, rect.y);
      float const x1 = std::min(clip->x + clip->width, rect.x + rect.width);
      float const y1 = std::min(clip->y + clip->height, rect.y + rect.height);
      if (x1 <= x0 || y1 <= y0) {
        clip = Rect::sharp(0.f, 0.f, 0.f, 0.f);
      } else {
        clip = Rect::sharp(x0, y0, x1 - x0, y1 - y0);
      }
    }
    target_.clipRect(rect, antiAlias);
  }

  Rect clipBounds() const override { return currentState().clip.value_or(target_.clipBounds()); }

  bool quickReject(Rect rect) const override {
    if (!currentState().clip.has_value()) {
      return false;
    }
    return !rect.intersects(*currentState().clip);
  }

  void setOpacity(float opacity) override {
    currentState().opacity = opacity;
    target_.setOpacity(opacity);
  }

  float opacity() const override { return currentState().opacity; }

  void setBlendMode(BlendMode mode) override {
    currentState().blendMode = mode;
    target_.setBlendMode(mode);
  }

  BlendMode blendMode() const override { return currentState().blendMode; }

  void drawRect(Rect const& rect, CornerRadius const& cornerRadius, FillStyle const& fill,
                StrokeStyle const& stroke, ShadowStyle const& shadow) override {
    target_.drawRect(rect, cornerRadius, fill, stroke, shadow);
  }

  void drawLine(Point from, Point to, StrokeStyle const& stroke) override {
    target_.drawLine(from, to, stroke);
  }

  void drawPath(Path const& path, FillStyle const& fill, StrokeStyle const& stroke,
                ShadowStyle const& shadow) override {
    target_.drawPath(path, fill, stroke, shadow);
  }

  void drawCircle(Point center, float radius, FillStyle const& fill, StrokeStyle const& stroke) override {
    target_.drawCircle(center, radius, fill, stroke);
  }

  void drawTextLayout(TextLayout const& layout, Point origin) override { target_.drawTextLayout(layout, origin); }

  void drawTextNode(TextNode const& node) override { target_.drawTextNode(node); }

  void drawImageNode(ImageNode const& node) override { target_.drawImageNode(node); }

  void drawImage(Image const& image, Rect const& src, Rect const& dst, CornerRadius const& corners,
                 float opacity) override {
    target_.drawImage(image, src, dst, corners, opacity);
  }

  void drawImageTiled(Image const& image, Rect const& dst, CornerRadius const& corners,
                      float opacity) override {
    target_.drawImageTiled(image, dst, corners, opacity);
  }

  void* gpuDevice() const override { return target_.gpuDevice(); }

  void clear(Color color = Colors::transparent) override { target_.clear(color); }

  void markUncacheable() noexcept { cacheable_ = false; }
  bool cacheable() const noexcept { return cacheable_; }

private:
  struct State {
    Mat3 transform = Mat3::identity();
    std::optional<Rect> clip;
    float opacity = 1.f;
    BlendMode blendMode = BlendMode::Normal;
  };

  State& currentState() { return stateStack_.back(); }
  State const& currentState() const { return stateStack_.back(); }

  Canvas& target_;
  std::vector<State> stateStack_{};
  bool cacheable_ = true;
};

struct LayerCacheKey {
  void const* graphIdentity = nullptr;
  NodeId node{};

  bool operator==(LayerCacheKey const& other) const {
    return graphIdentity == other.graphIdentity && node == other.node;
  }
};

struct LayerCacheKeyHash {
  std::size_t operator()(LayerCacheKey const& key) const noexcept {
    std::size_t h = std::hash<void const*>{}(key.graphIdentity);
    h ^= std::hash<std::uint32_t>{}(key.node.index) + 0x9e3779b9 + (h << 6U) + (h >> 2U);
    h ^= std::hash<std::uint32_t>{}(key.node.generation) + 0x9e3779b9 + (h << 6U) + (h >> 2U);
    return h;
  }
};

struct LayerCacheEntry {
  std::uint64_t epoch = 0;
  std::weak_ptr<void const> graphLifetime{};
  CanvasStateSnapshot canvasState{};
  MetalFrameRecorder recorded{};
  std::vector<LayerReplayStep> steps{};
};

bool flushRecordedBatch(Canvas& canvas, LayerCacheEntry& entry, MetalRecorderSlice& lastSlice) {
  MetalRecorderSlice slice{
      .orderStart = lastSlice.orderStart + lastSlice.orderCount,
      .orderCount = static_cast<std::uint32_t>(entry.recorded.opOrder.size()) -
                    (lastSlice.orderStart + lastSlice.orderCount),
      .rectStart = lastSlice.rectStart + lastSlice.rectCount,
      .rectCount = static_cast<std::uint32_t>(entry.recorded.rectOps.size()) -
                   (lastSlice.rectStart + lastSlice.rectCount),
      .imageStart = lastSlice.imageStart + lastSlice.imageCount,
      .imageCount = static_cast<std::uint32_t>(entry.recorded.imageOps.size()) -
                    (lastSlice.imageStart + lastSlice.imageCount),
      .pathOpStart = lastSlice.pathOpStart + lastSlice.pathOpCount,
      .pathOpCount = static_cast<std::uint32_t>(entry.recorded.pathOps.size()) -
                     (lastSlice.pathOpStart + lastSlice.pathOpCount),
      .glyphOpStart = lastSlice.glyphOpStart + lastSlice.glyphOpCount,
      .glyphOpCount = static_cast<std::uint32_t>(entry.recorded.glyphOps.size()) -
                      (lastSlice.glyphOpStart + lastSlice.glyphOpCount),
      .pathVertexStart = lastSlice.pathVertexStart + lastSlice.pathVertexCount,
      .pathVertexCount = static_cast<std::uint32_t>(entry.recorded.pathVerts.size()) -
                         (lastSlice.pathVertexStart + lastSlice.pathVertexCount),
      .glyphVertexStart = lastSlice.glyphVertexStart + lastSlice.glyphVertexCount,
      .glyphVertexCount = static_cast<std::uint32_t>(entry.recorded.glyphVerts.size()) -
                          (lastSlice.glyphVertexStart + lastSlice.glyphVertexCount),
  };
  if (slice.orderCount == 0 && slice.rectCount == 0 && slice.imageCount == 0 && slice.pathOpCount == 0 &&
      slice.glyphOpCount == 0 && slice.pathVertexCount == 0 && slice.glyphVertexCount == 0) {
    return false;
  }

  ReplayOpBatch batch{.slice = slice};
  entry.steps.emplace_back(batch);
  replayRecordedOpsForCanvas(&canvas, entry.recorded, batch.slice);
  lastSlice = slice;
  return true;
}

} // namespace

struct SceneRenderer::Impl {
  mutable std::unordered_map<LayerCacheKey, LayerCacheEntry, LayerCacheKeyHash> layerCache_{};
};

SceneRenderer::SceneRenderer()
    : impl_(std::make_unique<Impl>()) {}

SceneRenderer::~SceneRenderer() = default;
SceneRenderer::SceneRenderer(SceneRenderer&&) noexcept = default;
SceneRenderer& SceneRenderer::operator=(SceneRenderer&&) noexcept = default;

void SceneRenderer::render(SceneGraph const& graph, Canvas& canvas) const {
  void const* const graphIdentity = graph.cacheIdentity();
  std::erase_if(impl_->layerCache_, [&](auto const& entry) {
    if (entry.second.graphLifetime.expired()) {
      return true;
    }
    return entry.first.graphIdentity == graphIdentity && graph.get(entry.first.node) == nullptr;
  });
  renderNode(graph.root(), graph, canvas, true);
}

void SceneRenderer::render(SceneGraph const& graph, Canvas& canvas, Color clearColor) const {
  void const* const graphIdentity = graph.cacheIdentity();
  std::erase_if(impl_->layerCache_, [&](auto const& entry) {
    if (entry.second.graphLifetime.expired()) {
      return true;
    }
    return entry.first.graphIdentity == graphIdentity && graph.get(entry.first.node) == nullptr;
  });
  canvas.clear(clearColor);
  renderNode(graph.root(), graph, canvas, true);
}

void SceneRenderer::renderNode(NodeId id, SceneGraph const& graph, Canvas& canvas, bool allowLayerCache) const {
  SceneNode const* sn = graph.get(id);
  if (!sn) {
    return;
  }
  std::visit(
      [&](auto const& node) {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, LayerNode>) {
          renderLayer(node, graph, canvas, allowLayerCache);
        } else if constexpr (std::is_same_v<T, RectNode>) {
          canvas.drawRect(node.bounds, node.cornerRadius, node.fill, node.stroke, node.shadow);
        } else if constexpr (std::is_same_v<T, TextNode>) {
          canvas.drawTextNode(node);
        } else if constexpr (std::is_same_v<T, ImageNode>) {
          canvas.drawImageNode(node);
        } else if constexpr (std::is_same_v<T, PathNode>) {
          canvas.drawPath(node.path, node.fill, node.stroke, node.shadow);
        } else if constexpr (std::is_same_v<T, LineNode>) {
          canvas.drawLine(node.from, node.to, node.stroke);
        } else if constexpr (std::is_same_v<T, CustomRenderNode>) {
          if (auto* recorder = dynamic_cast<MetalRecordingCanvas*>(&canvas)) {
            recorder->markUncacheable();
          }
          canvas.save();
          if (node.draw) {
            node.draw(canvas);
          }
          canvas.restore();
        }
      },
      *sn);
}

void SceneRenderer::renderLayer(LayerNode const& layer, SceneGraph const& graph, Canvas& canvas,
                                bool allowLayerCache) const {
  canvas.save();
  canvas.transform(layer.transform);
  canvas.setOpacity(canvas.opacity() * layer.opacity);
  canvas.setBlendMode(layer.blendMode);
  if (layer.clip.has_value()) {
    canvas.clipRect(*layer.clip);
  }

  CanvasStateSnapshot const canvasState = captureCanvasState(canvas);
  LayerCacheKey const key{.graphIdentity = graph.cacheIdentity(), .node = layer.id};
  std::uint64_t const epoch = graph.subtreePaintEpoch(layer.id);

  if (allowLayerCache) {
    auto const it = impl_->layerCache_.find(key);
    if (it != impl_->layerCache_.end() && it->second.epoch == epoch &&
        canvasStateEqual(it->second.canvasState, canvasState)) {
      for (LayerReplayStep const& step : it->second.steps) {
        if (auto const* batch = std::get_if<ReplayOpBatch>(&step)) {
          replayRecordedOpsForCanvas(&canvas, it->second.recorded, batch->slice);
        } else if (auto const* child = std::get_if<RenderChildLayerCmd>(&step)) {
          renderNode(child->child, graph, canvas, true);
        }
      }
      canvas.restore();
      return;
    }
  }

  LayerCacheEntry cacheEntry;
  cacheEntry.epoch = epoch;
  cacheEntry.graphLifetime = graph.cacheLifetime();
  cacheEntry.canvasState = canvasState;

  MetalRecordingCanvas recorder(canvas);
  bool const captureSupported = allowLayerCache && beginRecordedOpsCaptureForCanvas(&canvas, &cacheEntry.recorded);
  bool captureActive = captureSupported;
  MetalRecorderSlice lastSlice{};

  for (NodeId childId : layer.children) {
    if (graph.node<LayerNode>(childId)) {
      if (captureActive) {
        endRecordedOpsCaptureForCanvas(&canvas);
        captureActive = false;
        flushRecordedBatch(canvas, cacheEntry, lastSlice);
      }
      cacheEntry.steps.emplace_back(RenderChildLayerCmd{.child = childId});
      renderNode(childId, graph, canvas, true);
      if (captureSupported) {
        beginRecordedOpsCaptureForCanvas(&canvas, &cacheEntry.recorded);
        captureActive = true;
      }
    } else if (captureSupported) {
      renderNode(childId, graph, recorder, false);
    } else {
      renderNode(childId, graph, canvas, false);
    }
  }

  if (captureActive) {
    endRecordedOpsCaptureForCanvas(&canvas);
    flushRecordedBatch(canvas, cacheEntry, lastSlice);
  }

  if (captureSupported && recorder.cacheable()) {
    impl_->layerCache_[key] = std::move(cacheEntry);
  } else {
    impl_->layerCache_.erase(key);
  }

  canvas.restore();
}

} // namespace flux
