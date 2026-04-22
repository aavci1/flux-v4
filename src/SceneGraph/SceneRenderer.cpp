#include <Flux/SceneGraph/SceneRenderer.hpp>

#include <Flux/Graphics/Canvas.hpp>
#include <Flux/SceneGraph/Renderer.hpp>
#include <Flux/SceneGraph/SceneGraph.hpp>
#include <Flux/SceneGraph/SceneNode.hpp>

namespace flux::scenegraph {

namespace {

class CanvasRenderer final : public Renderer {
public:
  explicit CanvasRenderer(Canvas& canvas)
      : canvas_(canvas) {}

  void save() override { canvas_.save(); }
  void restore() override { canvas_.restore(); }
  void translate(Point offset) override { canvas_.translate(offset); }
  void transform(Mat3 const& matrix) override { canvas_.transform(matrix); }
  void clipRect(Rect rect, CornerRadius const& cornerRadius, bool antiAlias) override {
    canvas_.clipRect(rect, cornerRadius, antiAlias);
  }
  bool quickReject(Rect rect) const override { return canvas_.quickReject(rect); }
  void setOpacity(float opacity) override { canvas_.setOpacity(opacity); }
  void setBlendMode(BlendMode mode) override { canvas_.setBlendMode(mode); }
  void drawRect(Rect const& rect, CornerRadius const& cornerRadius, FillStyle const& fill,
                StrokeStyle const& stroke, ShadowStyle const& shadow) override {
    canvas_.drawRect(rect, cornerRadius, fill, stroke, shadow);
  }
  void drawLine(Point from, Point to, StrokeStyle const& stroke) override {
    canvas_.drawLine(from, to, stroke);
  }
  void drawPath(Path const& path, FillStyle const& fill, StrokeStyle const& stroke,
                ShadowStyle const& shadow) override {
    canvas_.drawPath(path, fill, stroke, shadow);
  }
  void drawTextLayout(TextLayout const& layout, Point origin) override { canvas_.drawTextLayout(layout, origin); }
  void drawImage(Image const& image, Rect const& bounds, ImageFillMode fillMode,
                 CornerRadius const& cornerRadius, float opacity) override {
    canvas_.drawImage(image, bounds, fillMode, cornerRadius, opacity);
  }
  Canvas* canvas() noexcept override { return &canvas_; }

private:
  Canvas& canvas_;
};

} // namespace

SceneRenderer::SceneRenderer(Canvas& canvas)
    : renderer_(nullptr), ownedRenderer_(std::make_unique<CanvasRenderer>(canvas)) {
  renderer_ = ownedRenderer_.get();
}

SceneRenderer::SceneRenderer(Renderer& renderer)
    : renderer_(&renderer) {}

SceneRenderer::~SceneRenderer() = default;

void SceneRenderer::render(SceneGraph const& graph) {
  renderNode(graph.root());
}

void SceneRenderer::render(SceneNode const& node) {
  renderNode(node);
}

void SceneRenderer::renderNode(SceneNode const& node) {
  renderer_->save();
  renderer_->translate(Point{node.bounds.x, node.bounds.y});

  Rect const localBounds = node.localBounds();
  if (localBounds.width > 0.f && localBounds.height > 0.f && renderer_->quickReject(localBounds)) {
    renderer_->restore();
    return;
  }

  node.render(*renderer_);
  for (std::unique_ptr<SceneNode> const& child : node.children()) {
    renderNode(*child);
  }
  renderer_->restore();
}

} // namespace flux::scenegraph
