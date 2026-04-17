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
  std::uint32_t opStart = 0;
  std::uint32_t opCount = 0;
  std::uint32_t pathStart = 0;
  std::uint32_t pathCount = 0;
  std::uint32_t glyphStart = 0;
  std::uint32_t glyphCount = 0;
  bool hasPathOps = false;
  bool hasGlyphOps = false;
  bool hasImageOps = false;
};

using LayerReplayStep = std::variant<ReplayOpBatch, RenderChildLayerCmd>;

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
  SceneGraph const* graph = nullptr;
  NodeId node{};

  bool operator==(LayerCacheKey const& other) const {
    return graph == other.graph && node == other.node;
  }
};

struct LayerCacheKeyHash {
  std::size_t operator()(LayerCacheKey const& key) const noexcept {
    std::size_t h = std::hash<void const*>{}(key.graph);
    h ^= std::hash<std::uint32_t>{}(key.node.index) + 0x9e3779b9 + (h << 6U) + (h >> 2U);
    h ^= std::hash<std::uint32_t>{}(key.node.generation) + 0x9e3779b9 + (h << 6U) + (h >> 2U);
    return h;
  }
};

struct LayerCacheEntry {
  std::uint64_t epoch = 0;
  MetalFrameRecorder recorded{};
  std::vector<LayerReplayStep> steps{};
};

bool flushRecordedBatch(Canvas& canvas, LayerCacheEntry& entry, std::uint32_t& lastOp, std::uint32_t& lastPath,
                        std::uint32_t& lastGlyph) {
  std::uint32_t const opEnd = static_cast<std::uint32_t>(entry.recorded.ops.size());
  std::uint32_t const pathEnd = static_cast<std::uint32_t>(entry.recorded.pathVerts.size());
  std::uint32_t const glyphEnd = static_cast<std::uint32_t>(entry.recorded.glyphVerts.size());
  if (opEnd == lastOp && pathEnd == lastPath && glyphEnd == lastGlyph) {
    return false;
  }

  ReplayOpBatch batch{
      .opStart = lastOp,
      .opCount = opEnd - lastOp,
      .pathStart = lastPath,
      .pathCount = pathEnd - lastPath,
      .glyphStart = lastGlyph,
      .glyphCount = glyphEnd - lastGlyph,
  };
  for (std::uint32_t i = batch.opStart; i < opEnd; ++i) {
    switch (entry.recorded.ops[static_cast<std::size_t>(i)].kind) {
    case MetalDrawOp::PathMesh:
      batch.hasPathOps = true;
      break;
    case MetalDrawOp::GlyphMesh:
      batch.hasGlyphOps = true;
      break;
    case MetalDrawOp::Image:
      batch.hasImageOps = true;
      break;
    case MetalDrawOp::Rect:
    case MetalDrawOp::Line:
      break;
    }
  }
  entry.steps.emplace_back(batch);
  replayRecordedOpsForCanvas(&canvas, entry.recorded, batch.opStart, batch.opCount, batch.pathStart,
                             batch.pathCount, batch.glyphStart, batch.glyphCount, batch.hasPathOps,
                             batch.hasGlyphOps, batch.hasImageOps);
  lastOp = opEnd;
  lastPath = pathEnd;
  lastGlyph = glyphEnd;
  return true;
}

} // namespace

struct SceneRenderer::Impl {
  mutable std::unordered_map<LayerCacheKey, LayerCacheEntry, LayerCacheKeyHash> layerCache_{};
  mutable std::uint64_t renderCount_ = 0;
};

SceneRenderer::SceneRenderer()
    : impl_(std::make_unique<Impl>()) {}

SceneRenderer::~SceneRenderer() = default;
SceneRenderer::SceneRenderer(SceneRenderer&&) noexcept = default;
SceneRenderer& SceneRenderer::operator=(SceneRenderer&&) noexcept = default;

void SceneRenderer::render(SceneGraph const& graph, Canvas& canvas) const {
  if ((++impl_->renderCount_ & 1023U) == 0) {
    std::erase_if(impl_->layerCache_, [&](auto const& entry) {
      return entry.first.graph == &graph && graph.get(entry.first.node) == nullptr;
    });
  }
  renderNode(graph.root(), graph, canvas, true);
}

void SceneRenderer::render(SceneGraph const& graph, Canvas& canvas, Color clearColor) const {
  if ((++impl_->renderCount_ & 1023U) == 0) {
    std::erase_if(impl_->layerCache_, [&](auto const& entry) {
      return entry.first.graph == &graph && graph.get(entry.first.node) == nullptr;
    });
  }
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

  LayerCacheKey const key{.graph = &graph, .node = layer.id};
  std::uint64_t const epoch = graph.subtreePaintEpoch(layer.id);

  if (allowLayerCache) {
    auto const it = impl_->layerCache_.find(key);
    if (it != impl_->layerCache_.end() && it->second.epoch == epoch) {
      for (LayerReplayStep const& step : it->second.steps) {
        if (auto const* batch = std::get_if<ReplayOpBatch>(&step)) {
          replayRecordedOpsForCanvas(&canvas, it->second.recorded, batch->opStart, batch->opCount,
                                     batch->pathStart, batch->pathCount, batch->glyphStart, batch->glyphCount,
                                     batch->hasPathOps, batch->hasGlyphOps, batch->hasImageOps);
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

  MetalRecordingCanvas recorder(canvas);
  bool const captureSupported = allowLayerCache && beginRecordedOpsCaptureForCanvas(&canvas, &cacheEntry.recorded);
  bool captureActive = captureSupported;
  std::uint32_t lastOp = 0;
  std::uint32_t lastPath = 0;
  std::uint32_t lastGlyph = 0;

  for (NodeId childId : layer.children) {
    if (graph.node<LayerNode>(childId)) {
      if (captureActive) {
        endRecordedOpsCaptureForCanvas(&canvas);
        captureActive = false;
        flushRecordedBatch(canvas, cacheEntry, lastOp, lastPath, lastGlyph);
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
    flushRecordedBatch(canvas, cacheEntry, lastOp, lastPath, lastGlyph);
  }

  if (captureSupported && recorder.cacheable()) {
    impl_->layerCache_[key] = std::move(cacheEntry);
  } else {
    impl_->layerCache_.erase(key);
  }

  canvas.restore();
}

} // namespace flux
