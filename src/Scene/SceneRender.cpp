#include <Flux/Scene/Render.hpp>

#include <Flux/Graphics/Canvas.hpp>
#include <Flux/Scene/Renderer.hpp>
#include <Flux/Scene/SceneTree.hpp>

namespace flux {

namespace {

class CanvasRenderer final : public Renderer {
public:
  explicit CanvasRenderer(Canvas& canvas)
      : canvas_(canvas) {}

  void save() override { canvas_.save(); }
  void restore() override { canvas_.restore(); }
  void translate(Point offset) override { canvas_.translate(offset); }
  void transform(Mat3 const& matrix) override { canvas_.transform(matrix); }
  void clipRect(Rect rect, CornerRadius const& cornerRadius, bool antiAlias = false) override {
    canvas_.clipRect(rect, cornerRadius, antiAlias);
  }
  bool quickReject(Rect rect) const override { return canvas_.quickReject(rect); }
  void setOpacity(float opacity) override { canvas_.setOpacity(opacity); }
  void setBlendMode(BlendMode mode) override { canvas_.setBlendMode(mode); }
  void drawRect(Rect const& rect, CornerRadius const& cornerRadius, FillStyle const& fill,
                StrokeStyle const& stroke, ShadowStyle const& shadow) override {
    canvas_.drawRect(rect, cornerRadius, fill, stroke, shadow);
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

void renderNode(SceneNode& node, Renderer& renderer) {
  renderer.save();
  renderer.translate(node.position);
  node.applyNodeState(renderer);
  if (renderer.quickReject(node.bounds)) {
    renderer.restore();
    return;
  }
  if (node.paints()) {
    if (node.paintDirty()) {
      node.rebuildLocalPaint();
    }
    node.replayLocalPaint(renderer);
  }
  for (std::unique_ptr<SceneNode> const& child : node.children()) {
    renderNode(*child, renderer);
  }
  renderer.restore();
}

void renderNode(SceneNode const& node, Renderer& renderer) {
  renderNode(const_cast<SceneNode&>(node), renderer);
}

} // namespace

void render(SceneNode& node, Renderer& renderer) {
  renderNode(node, renderer);
}

void render(SceneNode const& node, Renderer& renderer) {
  renderNode(node, renderer);
}

void render(SceneTree& tree, Renderer& renderer) {
  renderNode(tree.root(), renderer);
}

void render(SceneTree const& tree, Renderer& renderer) {
  renderNode(tree.root(), renderer);
}

void render(SceneTree& tree, Canvas& canvas) {
  CanvasRenderer renderer{canvas};
  renderNode(tree.root(), renderer);
}

void render(SceneTree const& tree, Canvas& canvas) {
  CanvasRenderer renderer{canvas};
  renderNode(tree.root(), renderer);
}

} // namespace flux
