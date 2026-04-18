#include <Flux.hpp>
#include <Flux/Core/Events.hpp>
#include <Flux/Graphics/Canvas.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/Scene/CustomTransformSceneNode.hpp>
#include <Flux/Scene/HitTester.hpp>
#include <Flux/Scene/LineSceneNode.hpp>
#include <Flux/Scene/ModifierSceneNode.hpp>
#include <Flux/Scene/RectSceneNode.hpp>
#include <Flux/Scene/SceneTree.hpp>
#include <Flux/Scene/SceneTreeDump.hpp>
#include <Flux/Scene/TextSceneNode.hpp>
#include <Flux/UI/Theme.hpp>

#include <algorithm>
#include <cassert>
#include <iostream>
#include <optional>
#include <string>

using namespace flux;

namespace {

char const* describeHit(NodeId id, NodeId bg, NodeId cardA, NodeId cardB, NodeId cardC, NodeId title,
                        NodeId hint) {
  if (!id.isValid()) {
    return "—";
  }
  if (id == bg) {
    return "Background (rect)";
  }
  if (id == cardA) {
    return "Card A (rect)";
  }
  if (id == cardB) {
    return "Card B (rect)";
  }
  if (id == cardC) {
    return "Card C (rect)";
  }
  if (id == title) {
    return "Title (text)";
  }
  if (id == hint) {
    return "Hint (text)";
  }
  return "Node (layer / line)";
}

class SceneDemoWindow : public Window {
  bool sceneBuilt_ = false;
  Size lastLayout_{};

  static constexpr NodeId rootId_{1ull};
  static constexpr NodeId bgId_{2ull};
  static constexpr NodeId transformId_{3ull};
  static constexpr NodeId clipId_{4ull};
  static constexpr NodeId cardA_{5ull};
  static constexpr NodeId cardB_{6ull};
  static constexpr NodeId cardC_{7ull};
  static constexpr NodeId lineId_{8ull};
  static constexpr NodeId titleId_{9ull};
  static constexpr NodeId hintId_{10ull};

  std::optional<NodeId> lastHover_{};

  static SceneNode* findNodeById(SceneNode& node, NodeId id) {
    if (node.id() == id) {
      return &node;
    }
    for (std::unique_ptr<SceneNode> const& child : node.children()) {
      if (SceneNode* found = findNodeById(*child, id)) {
        return found;
      }
    }
    return nullptr;
  }

  template <typename T>
  static T* findNode(SceneTree& tree, NodeId id) {
    return dynamic_cast<T*>(findNodeById(tree.root(), id));
  }

public:
  explicit SceneDemoWindow(WindowConfig const& c) : Window(c) {
    Application::instance().eventQueue().on<WindowEvent>([this](WindowEvent const& ev) {
      if (ev.handle != handle() || ev.kind != WindowEvent::Kind::Resize) {
        return;
      }
      // Geometry will be rebuilt in `render`; clear hover label so we do not show hits against stale bounds.
      lastHover_ = std::nullopt;
      setTitle("Flux — Scene tree — hover: —");
    });
    Application::instance().eventQueue().on<InputEvent>([this](InputEvent const& e) {
      if (e.handle != handle()) {
        return;
      }
      if (e.kind != InputEvent::Kind::PointerMove && e.kind != InputEvent::Kind::PointerDown &&
          e.kind != InputEvent::Kind::PointerUp) {
        return;
      }
      Point const p{e.position.x, e.position.y};
      if (!hasSceneTree()) {
        return;
      }
      auto const hit = HitTester{}.hitTest(sceneTree(), p);
      std::optional<NodeId> const curOpt = hit ? std::optional<NodeId>(hit->nodeId) : std::nullopt;
      if (e.kind == InputEvent::Kind::PointerMove && curOpt == lastHover_) {
        return;
      }
      lastHover_ = curOpt;
      NodeId const cur = hit ? hit->nodeId : kInvalidNodeId;

      char const* label = describeHit(cur, bgId_, cardA_, cardB_, cardC_, titleId_, hintId_);
      std::string t = "Flux — Scene tree — hover: ";
      t += label;
      if (e.kind == InputEvent::Kind::PointerDown && hit) {
        t += " (down)";
      }
      setTitle(t);
    });
  }

  void buildScene(Size const& s) {
    float const margin = 28.f;
    TextSystem& ts = Application::instance().textSystem();
    Theme const theme = Theme::dark();
    Font const titleFont = theme.fontDisplay;
    Font const hintFont = theme.fontBodySmall;
    auto titleLayout =
        ts.layout("Scene tree", titleFont, theme.colorTextPrimary, s.width - margin * 2.f);
    float const titleBand = titleLayout->measuredSize.height;
    float const rowTop = margin + titleBand + 18.f;
    float const rowH = 160.f;
    float const gap = 18.f;

    auto root = std::make_unique<SceneNode>(rootId_);

    auto bg = std::make_unique<RectSceneNode>(bgId_);
    bg->size = Size{s.width, s.height};
    bg->fill = FillStyle::solid(Color::hex(0x1e2229));
    bg->stroke = StrokeStyle::none();
    bg->cornerRadius = {};
    bg->recomputeBounds();
    root->appendChild(std::move(bg));

    float const contentW = std::max(s.width - margin * 2.f, 120.f);
    float const cardW = (contentW - gap * 2.f) / 3.f;
    Point const rowOrigin{margin, rowTop};

    Point const pivot{s.width * 0.5f, rowTop + rowH * 0.5f};
    float const rot = 0.055f;
    Mat3 const transform =
        Mat3::translate(pivot) * Mat3::rotate(rot) * Mat3::translate(Point{-pivot.x, -pivot.y});

    auto transformedGroup = std::make_unique<CustomTransformSceneNode>(transformId_);
    transformedGroup->transform = transform;

    auto clipGroup = std::make_unique<ModifierSceneNode>(clipId_);
    clipGroup->opacity = 0.98f;
    clipGroup->clip = Rect::sharp(margin - 8.f, rowTop - 8.f, contentW + 16.f, rowH + 16.f);

    auto makeCard = [&](NodeId id, float x, float y, float w, float h, Color fill) {
      auto card = std::make_unique<RectSceneNode>(id);
      card->position = Point{x, y};
      card->size = Size{w, h};
      card->cornerRadius = CornerRadius(14.f);
      card->fill = FillStyle::solid(fill);
      card->stroke = StrokeStyle::solid(Color::rgb(255, 255, 255), 1.6f);
      card->recomputeBounds();
      return card;
    };

    clipGroup->appendChild(makeCard(cardA_, rowOrigin.x, rowOrigin.y, cardW, rowH, Color::hex(0x3d5a80)));
    clipGroup->appendChild(makeCard(cardB_, rowOrigin.x + cardW + gap, rowOrigin.y, cardW, rowH,
                                    Color::hex(0x5a7d3a)));
    clipGroup->appendChild(makeCard(cardC_, rowOrigin.x + 2.f * (cardW + gap), rowOrigin.y, cardW, rowH,
                                    Color::hex(0x8a4a6a)));

    auto line = std::make_unique<LineSceneNode>(lineId_);
    line->from = Point{rowOrigin.x + cardW * 0.85f, rowOrigin.y + rowH * 0.72f};
    line->to = Point{rowOrigin.x + cardW + gap + cardW * 0.15f, rowOrigin.y + rowH * 0.72f};
    line->stroke = StrokeStyle::solid(Color::rgb(255, 255, 255), 3.f);
    line->recomputeBounds();
    clipGroup->appendChild(std::move(line));

    clipGroup->recomputeBounds();
    transformedGroup->appendChild(std::move(clipGroup));
    transformedGroup->recomputeBounds();
    root->appendChild(std::move(transformedGroup));

    float const titleW = titleLayout->measuredSize.width;
    float const titleX = margin + (s.width - margin * 2.f - titleW) * 0.5f;
    auto title = std::make_unique<TextSceneNode>(titleId_);
    title->textSystem = &ts;
    title->layout = std::move(titleLayout);
    title->position = Point{titleX, margin};
    title->origin = Point{0.f, 0.f};
    title->recomputeBounds();
    root->appendChild(std::move(title));

    auto hintLayout =
        ts.layout("Transform, clip, rects, line, text, hit-test", hintFont, theme.colorTextMuted,
                  s.width - margin * 2.f);
    float const hintW = hintLayout->measuredSize.width;
    float const hintX = margin + (s.width - margin * 2.f - hintW) * 0.5f;
    auto hint = std::make_unique<TextSceneNode>(hintId_);
    hint->textSystem = &ts;
    hint->layout = std::move(hintLayout);
    hint->position = Point{hintX, margin + titleBand + 6.f};
    hint->origin = Point{0.f, 0.f};
    hint->recomputeBounds();
    root->appendChild(std::move(hint));

    root->recomputeBounds();
    sceneTree().setRoot(std::move(root));

    sceneBuilt_ = true;
    lastLayout_ = s;
  }

  void layoutScene(Size const& s) {
    float const margin = 28.f;
    TextSystem& ts = Application::instance().textSystem();
    Theme const theme = Theme::dark();
    Font const titleFont = theme.fontDisplay;
    Font const hintFont = theme.fontBodySmall;
    float const titleBand =
        ts.layout("Scene tree", titleFont, theme.colorTextPrimary, s.width - margin * 2.f)->measuredSize.height;
    float const rowTop = margin + titleBand + 18.f;
    float const rowH = 160.f;
    float const gap = 18.f;

    SceneTree& tree = sceneTree();
    auto* bg = findNode<RectSceneNode>(tree, bgId_);
    auto* transformNode = findNode<CustomTransformSceneNode>(tree, transformId_);
    auto* clipNode = findNode<ModifierSceneNode>(tree, clipId_);
    auto* cardA = findNode<RectSceneNode>(tree, cardA_);
    auto* cardB = findNode<RectSceneNode>(tree, cardB_);
    auto* cardC = findNode<RectSceneNode>(tree, cardC_);
    auto* line = findNode<LineSceneNode>(tree, lineId_);
    auto* title = findNode<TextSceneNode>(tree, titleId_);
    auto* hint = findNode<TextSceneNode>(tree, hintId_);

    assert(bg && transformNode && clipNode && cardA && cardB && cardC && line && title && hint);

    bg->size = Size{s.width, s.height};
    bg->invalidatePaint();
    bg->markBoundsDirty();
    bg->recomputeBounds();

    float const contentW = std::max(s.width - margin * 2.f, 120.f);
    float const cardW = (contentW - gap * 2.f) / 3.f;
    Point const rowOrigin{margin, rowTop};

    Point const pivot{s.width * 0.5f, rowTop + rowH * 0.5f};
    float const rot = 0.055f;
    transformNode->transform =
        Mat3::translate(pivot) * Mat3::rotate(rot) * Mat3::translate(Point{-pivot.x, -pivot.y});
    transformNode->markBoundsDirty();

    clipNode->clip = Rect::sharp(margin - 8.f, rowTop - 8.f, contentW + 16.f, rowH + 16.f);
    clipNode->markBoundsDirty();

    auto layoutCard = [&](RectSceneNode* card, float x) {
      card->position = Point{x, rowOrigin.y};
      card->size = Size{cardW, rowH};
      card->invalidatePaint();
      card->markBoundsDirty();
      card->recomputeBounds();
    };
    layoutCard(cardA, rowOrigin.x);
    layoutCard(cardB, rowOrigin.x + cardW + gap);
    layoutCard(cardC, rowOrigin.x + 2.f * (cardW + gap));

    line->from = Point{rowOrigin.x + cardW * 0.85f, rowOrigin.y + rowH * 0.72f};
    line->to = Point{rowOrigin.x + cardW + gap + cardW * 0.15f, rowOrigin.y + rowH * 0.72f};
    line->invalidatePaint();
    line->markBoundsDirty();
    line->recomputeBounds();

    title->layout = ts.layout("Scene tree", titleFont, theme.colorTextPrimary, s.width - margin * 2.f);
    title->position = Point{margin + (s.width - margin * 2.f - title->layout->measuredSize.width) * 0.5f, margin};
    title->invalidatePaint();
    title->markBoundsDirty();
    title->recomputeBounds();

    hint->layout = ts.layout("Transform, clip, rects, line, text, hit-test", hintFont,
                             theme.colorTextMuted, s.width - margin * 2.f);
    hint->position =
        Point{margin + (s.width - margin * 2.f - hint->layout->measuredSize.width) * 0.5f, margin + titleBand + 6.f};
    hint->invalidatePaint();
    hint->markBoundsDirty();
    hint->recomputeBounds();

    clipNode->recomputeBounds();
    transformNode->recomputeBounds();
    tree.root().recomputeBounds();

    lastLayout_ = s;
  }

  void render(Canvas& canvas) override {
    // Match hello-world: clipBounds() is valid before the view reports a non-zero size; getSize() can lag.
    Rect const vb = canvas.clipBounds();
    Size const sz = getSize();
    float const w = std::max({vb.width, sz.width, 1.f});
    float const h = std::max({vb.height, sz.height, 1.f});
    Size const layout{w, h};
    if (layout.width < 1.f || layout.height < 1.f) {
      return;
    }
    if (!sceneBuilt_) {
      buildScene(layout);
      dumpSceneTree(sceneTree(), std::cerr);
    } else if (layout.width != lastLayout_.width || layout.height != lastLayout_.height) {
      layoutScene(layout);
      dumpSceneTree(sceneTree(), std::cerr);
    }

    canvas.clear(Color::hex(0x1a1d24));
    flux::render(sceneTree(), canvas);
  }
};

} // namespace

int main(int argc, char* argv[]) {
  Application app(argc, argv);

  app.createWindow<SceneDemoWindow>({
      .size = {720, 480},
      .title = "Flux — Scene tree demo",
      .resizable = true,
  });

  return app.exec();
}
