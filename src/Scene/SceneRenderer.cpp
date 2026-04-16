#include <Flux/Scene/SceneRenderer.hpp>

#include <Flux/Graphics/Canvas.hpp>
#include <Flux/Scene/Nodes.hpp>
#include <Flux/Scene/SceneGraph.hpp>

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace flux {

namespace {

struct SaveCmd {};
struct RestoreCmd {};
struct SetTransformCmd {
  Mat3 transform{};
};
struct TransformCmd {
  Mat3 transform{};
};
struct ClipRectCmd {
  Rect rect{};
  bool antiAlias = false;
};
struct SetOpacityCmd {
  float opacity = 1.f;
};
struct SetBlendModeCmd {
  BlendMode blendMode = BlendMode::Normal;
};
struct DrawRectCmd {
  Rect bounds{};
  CornerRadius cornerRadius{};
  FillStyle fill = FillStyle::none();
  StrokeStyle stroke = StrokeStyle::none();
  ShadowStyle shadow = ShadowStyle::none();
};
struct DrawLineCmd {
  Point from{};
  Point to{};
  StrokeStyle stroke = StrokeStyle::none();
};
struct DrawPathCmd {
  Path path{};
  FillStyle fill = FillStyle::none();
  StrokeStyle stroke = StrokeStyle::none();
  ShadowStyle shadow = ShadowStyle::none();
};
struct DrawCircleCmd {
  Point center{};
  float radius = 0.f;
  FillStyle fill = FillStyle::none();
  StrokeStyle stroke = StrokeStyle::none();
};
struct DrawTextNodeCmd {
  TextNode node{};
};
struct DrawImageNodeCmd {
  ImageNode node{};
};
struct DrawCustomClearCmd {
  Color color = Colors::transparent;
};
struct RenderChildLayerCmd {
  NodeId child{};
};

using RecordedCommand = std::variant<SaveCmd, RestoreCmd, SetTransformCmd, TransformCmd, ClipRectCmd,
                                     SetOpacityCmd, SetBlendModeCmd, DrawRectCmd, DrawLineCmd,
                                     DrawPathCmd, DrawCircleCmd, DrawTextNodeCmd, DrawImageNodeCmd,
                                     DrawCustomClearCmd, RenderChildLayerCmd>;

class RecordingCanvas final : public Canvas {
public:
  explicit RecordingCanvas(Canvas& target)
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
    commands_.emplace_back(SaveCmd{});
    target_.save();
  }

  void restore() override {
    if (stateStack_.size() > 1) {
      stateStack_.pop_back();
    }
    commands_.emplace_back(RestoreCmd{});
    target_.restore();
  }

  void setTransform(Mat3 const& m) override {
    currentState().transform = m;
    commands_.emplace_back(SetTransformCmd{.transform = m});
    target_.setTransform(m);
  }

  void transform(Mat3 const& m) override {
    currentState().transform = currentState().transform * m;
    commands_.emplace_back(TransformCmd{.transform = m});
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
    commands_.emplace_back(ClipRectCmd{.rect = rect, .antiAlias = antiAlias});
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
    commands_.emplace_back(SetOpacityCmd{.opacity = opacity});
    target_.setOpacity(opacity);
  }

  float opacity() const override { return currentState().opacity; }

  void setBlendMode(BlendMode mode) override {
    currentState().blendMode = mode;
    commands_.emplace_back(SetBlendModeCmd{.blendMode = mode});
    target_.setBlendMode(mode);
  }

  BlendMode blendMode() const override { return currentState().blendMode; }

  void drawRect(Rect const& rect, CornerRadius const& cornerRadius, FillStyle const& fill,
                StrokeStyle const& stroke, ShadowStyle const& shadow) override {
    commands_.emplace_back(DrawRectCmd{
        .bounds = rect,
        .cornerRadius = cornerRadius,
        .fill = fill,
        .stroke = stroke,
        .shadow = shadow,
    });
    target_.drawRect(rect, cornerRadius, fill, stroke, shadow);
  }

  void drawLine(Point from, Point to, StrokeStyle const& stroke) override {
    commands_.emplace_back(DrawLineCmd{.from = from, .to = to, .stroke = stroke});
    target_.drawLine(from, to, stroke);
  }

  void drawPath(Path const& path, FillStyle const& fill, StrokeStyle const& stroke,
                ShadowStyle const& shadow) override {
    commands_.emplace_back(DrawPathCmd{
        .path = path,
        .fill = fill,
        .stroke = stroke,
        .shadow = shadow,
    });
    target_.drawPath(path, fill, stroke, shadow);
  }

  void drawCircle(Point center, float radius, FillStyle const& fill, StrokeStyle const& stroke) override {
    commands_.emplace_back(DrawCircleCmd{
        .center = center,
        .radius = radius,
        .fill = fill,
        .stroke = stroke,
    });
    target_.drawCircle(center, radius, fill, stroke);
  }

  void drawTextLayout(TextLayout const& layout, Point origin) override {
    cacheable_ = false;
    target_.drawTextLayout(layout, origin);
  }

  void drawTextNode(TextNode const& node) override {
    commands_.emplace_back(DrawTextNodeCmd{.node = node});
    target_.drawTextNode(node);
  }

  void drawImageNode(ImageNode const& node) override {
    commands_.emplace_back(DrawImageNodeCmd{.node = node});
    target_.drawImageNode(node);
  }

  void drawImage(Image const& image, Rect const& src, Rect const& dst, CornerRadius const& corners,
                 float opacity) override {
    cacheable_ = false;
    target_.drawImage(image, src, dst, corners, opacity);
  }

  void drawImageTiled(Image const& image, Rect const& dst, CornerRadius const& corners,
                      float opacity) override {
    cacheable_ = false;
    target_.drawImageTiled(image, dst, corners, opacity);
  }

  void* gpuDevice() const override { return target_.gpuDevice(); }

  void clear(Color color = Colors::transparent) override {
    commands_.emplace_back(DrawCustomClearCmd{.color = color});
    target_.clear(color);
  }

  void markUncacheable() noexcept { cacheable_ = false; }
  bool cacheable() const noexcept { return cacheable_; }
  void drainInto(std::vector<RecordedCommand>& out) {
    out.insert(out.end(),
               std::make_move_iterator(commands_.begin()),
               std::make_move_iterator(commands_.end()));
    commands_.clear();
  }

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
  std::vector<RecordedCommand> commands_{};
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
  std::vector<RecordedCommand> commands{};
};

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
  renderNode(graph.root(), graph, canvas, true);
}

void SceneRenderer::render(SceneGraph const& graph, Canvas& canvas, Color clearColor) const {
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
          if (auto* recorder = dynamic_cast<RecordingCanvas*>(&canvas)) {
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
      for (RecordedCommand const& command : it->second.commands) {
        if (auto const* child = std::get_if<RenderChildLayerCmd>(&command)) {
          renderNode(child->child, graph, canvas, true);
        } else {
          std::visit(
              [&](auto const& cmd) {
                using T = std::decay_t<decltype(cmd)>;
                if constexpr (std::is_same_v<T, SaveCmd>) {
                  canvas.save();
                } else if constexpr (std::is_same_v<T, RestoreCmd>) {
                  canvas.restore();
                } else if constexpr (std::is_same_v<T, SetTransformCmd>) {
                  canvas.setTransform(cmd.transform);
                } else if constexpr (std::is_same_v<T, TransformCmd>) {
                  canvas.transform(cmd.transform);
                } else if constexpr (std::is_same_v<T, ClipRectCmd>) {
                  canvas.clipRect(cmd.rect, cmd.antiAlias);
                } else if constexpr (std::is_same_v<T, SetOpacityCmd>) {
                  canvas.setOpacity(cmd.opacity);
                } else if constexpr (std::is_same_v<T, SetBlendModeCmd>) {
                  canvas.setBlendMode(cmd.blendMode);
                } else if constexpr (std::is_same_v<T, DrawRectCmd>) {
                  canvas.drawRect(cmd.bounds, cmd.cornerRadius, cmd.fill, cmd.stroke, cmd.shadow);
                } else if constexpr (std::is_same_v<T, DrawLineCmd>) {
                  canvas.drawLine(cmd.from, cmd.to, cmd.stroke);
                } else if constexpr (std::is_same_v<T, DrawPathCmd>) {
                  canvas.drawPath(cmd.path, cmd.fill, cmd.stroke, cmd.shadow);
                } else if constexpr (std::is_same_v<T, DrawCircleCmd>) {
                  canvas.drawCircle(cmd.center, cmd.radius, cmd.fill, cmd.stroke);
                } else if constexpr (std::is_same_v<T, DrawTextNodeCmd>) {
                  canvas.drawTextNode(cmd.node);
                } else if constexpr (std::is_same_v<T, DrawImageNodeCmd>) {
                  canvas.drawImageNode(cmd.node);
                } else if constexpr (std::is_same_v<T, DrawCustomClearCmd>) {
                  canvas.clear(cmd.color);
                } else if constexpr (std::is_same_v<T, RenderChildLayerCmd>) {
                }
              },
              command);
        }
      }
      canvas.restore();
      return;
    }
  }

  std::vector<RecordedCommand> commands;
  RecordingCanvas recorder(canvas);
  for (NodeId childId : layer.children) {
    if (graph.node<LayerNode>(childId)) {
      recorder.drainInto(commands);
      commands.emplace_back(RenderChildLayerCmd{.child = childId});
      renderNode(childId, graph, canvas, true);
    } else {
      renderNode(childId, graph, recorder, false);
    }
  }
  recorder.drainInto(commands);

  if (recorder.cacheable()) {
    impl_->layerCache_[key] = LayerCacheEntry{
        .epoch = epoch,
        .commands = std::move(commands),
    };
  } else {
    impl_->layerCache_.erase(key);
  }

  canvas.restore();
}

} // namespace flux
