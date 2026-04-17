#include <Flux/Scene/SceneTree.hpp>

#include <Flux/Scene/ModifierSceneNode.hpp>
#include <Flux/Scene/RenderSceneNode.hpp>

#include <Flux/Graphics/Canvas.hpp>

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
  void clipRect(Rect rect, bool antiAlias = false) override { canvas_.clipRect(rect, antiAlias); }
  bool quickReject(Rect rect) const override { return canvas_.quickReject(rect); }
  void setOpacity(float opacity) override { canvas_.setOpacity(opacity); }
  void setBlendMode(BlendMode mode) override { canvas_.setBlendMode(mode); }
  void drawRect(Rect const& rect, CornerRadius const& cornerRadius, FillStyle const& fill,
                StrokeStyle const& stroke, ShadowStyle const& shadow) override {
    canvas_.drawRect(rect, cornerRadius, fill, stroke, shadow);
  }
  void drawLine(Point from, Point to, StrokeStyle const& stroke) override { canvas_.drawLine(from, to, stroke); }
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

Rect unionRect(Rect lhs, Rect rhs) {
  if (lhs.width == 0.f && lhs.height == 0.f) {
    return rhs;
  }
  if (rhs.width == 0.f && rhs.height == 0.f) {
    return lhs;
  }
  float const x0 = std::min(lhs.x, rhs.x);
  float const y0 = std::min(lhs.y, rhs.y);
  float const x1 = std::max(lhs.x + lhs.width, rhs.x + rhs.width);
  float const y1 = std::max(lhs.y + lhs.height, rhs.y + rhs.height);
  return Rect{x0, y0, x1 - x0, y1 - y0};
}

Rect offsetRect(Rect rect, Point delta) {
  rect.x += delta.x;
  rect.y += delta.y;
  return rect;
}

std::uint64_t hashCombine64(std::uint64_t seed, std::uint64_t value) {
  seed ^= value + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
  return seed;
}

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
    if (auto* renderNodePtr = dynamic_cast<RenderSceneNode*>(&node)) {
      if (Canvas* canvas = renderer.canvas(); canvas && renderNodePtr->draw) {
        renderNodePtr->draw(*canvas, renderNodePtr->frame);
      }
    } else {
      node.replayLocalPaint(renderer);
    }
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

SceneTree::SceneTree()
    : root_(std::make_unique<SceneNode>(NodeId{1ull})) {}

SceneTree::SceneTree(std::unique_ptr<SceneNode> root)
    : root_(std::move(root)) {
  if (!root_) {
    root_ = std::make_unique<SceneNode>(NodeId{1ull});
  }
}

SceneTree::~SceneTree() = default;
SceneTree::SceneTree(SceneTree&&) noexcept = default;
SceneTree& SceneTree::operator=(SceneTree&&) noexcept = default;

std::unique_ptr<SceneNode> SceneTree::takeRoot() {
  std::unique_ptr<SceneNode> root = std::move(root_);
  root_ = std::make_unique<SceneNode>(NodeId{1ull});
  return root;
}

void SceneTree::setRoot(std::unique_ptr<SceneNode> root) {
  root_ = std::move(root);
  if (!root_) {
    root_ = std::make_unique<SceneNode>(NodeId{1ull});
  }
}

void SceneTree::clear() {
  root_ = std::make_unique<SceneNode>(NodeId{1ull});
}

NodeId SceneTree::childId(NodeId parent, LocalId local) noexcept {
  std::uint64_t seed = parent.value == 0 ? 0x9f47b2d1aa61c6e1ull : parent.value;
  seed = hashCombine64(seed, static_cast<std::uint64_t>(local.kind));
  seed = hashCombine64(seed, local.value);
  if (seed == 0) {
    seed = 1;
  }
  return NodeId{seed};
}

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

Rect measureRootContentBounds(SceneTree const& tree) {
  Rect bounds{};
  for (std::unique_ptr<SceneNode> const& child : tree.root().children()) {
    bounds = unionRect(bounds, offsetRect(child->bounds, child->position));
  }
  return bounds;
}

Size measureRootContentSize(SceneTree const& tree) {
  Rect const bounds = measureRootContentBounds(tree);
  return Size{bounds.width, bounds.height};
}

} // namespace flux
