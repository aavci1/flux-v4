#include <Flux.hpp>
#include <Flux/Core/Events.hpp>
#include <Flux/Graphics/Canvas.hpp>
#include <Flux/Graphics/Font.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/Scene/HitTester.hpp>
#include <Flux/Scene/Nodes.hpp>
#include <Flux/Scene/SceneGraph.hpp>
#include <Flux/Scene/SceneGraphDump.hpp>
#include <Flux/Scene/SceneRenderer.hpp>

#include <algorithm>
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

  NodeId bgId_{};
  NodeId groupId_{};
  NodeId cardA_{};
  NodeId cardB_{};
  NodeId cardC_{};
  NodeId lineId_{};
  NodeId titleId_{};
  NodeId hintId_{};

  std::optional<NodeId> lastHover_{};

public:
  explicit SceneDemoWindow(WindowConfig const& c) : Window(c) {
    sceneGraph();
    Application::instance().eventQueue().on<WindowEvent>([this](WindowEvent const& ev) {
      if (ev.handle != handle() || ev.kind != WindowEvent::Kind::Resize) {
        return;
      }
      // Geometry will be rebuilt in `render`; clear hover label so we do not show hits against stale bounds.
      lastHover_ = std::nullopt;
      setTitle("Flux — Scene graph — hover: —");
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
      auto const hit = HitTester{}.hitTest(sceneGraph(), p);
      std::optional<NodeId> const curOpt = hit ? std::optional<NodeId>(hit->nodeId) : std::nullopt;
      if (e.kind == InputEvent::Kind::PointerMove && curOpt == lastHover_) {
        return;
      }
      lastHover_ = curOpt;
      NodeId const cur = hit ? hit->nodeId : kInvalidNodeId;

      char const* label = describeHit(cur, bgId_, cardA_, cardB_, cardC_, titleId_, hintId_);
      std::string t = "Flux — Scene graph — hover: ";
      t += label;
      if (e.kind == InputEvent::Kind::PointerDown && hit) {
        t += " (down)";
      }
      setTitle(t);
    });
  }

  void buildScene(Size const& s) {
    SceneGraph& g = sceneGraph();
    NodeId const root = g.root();

    float const margin = 28.f;
    TextSystem& ts = Application::instance().textSystem();
    Font titleFont{.family = ".AppleSystemUIFont", .size = 26.f, .weight = 600.f};
    auto titleLayout = ts.layout("Scene graph", titleFont, Color::rgb(235, 238, 245), s.width - margin * 2.f);
    float const titleBand = titleLayout->measuredSize.height;
    float const rowTop = margin + titleBand + 18.f;
    float const rowH = 160.f;
    float const gap = 18.f;

    Rect const full = Rect::sharp(0.f, 0.f, s.width, s.height);
    bgId_ = g.addRect(root, RectNode{.bounds = full,
                                     .cornerRadius = {},
                                     .fill = FillStyle::solid(Color::hex(0x1e2229)),
                                     .stroke = StrokeStyle::none()});

    float const contentW = std::max(s.width - margin * 2.f, 120.f);
    float const cardW = (contentW - gap * 2.f) / 3.f;
    Point const rowOrigin{margin, rowTop};

    Point const pivot{s.width * 0.5f, rowTop + rowH * 0.5f};
    float const rot = 0.055f;
    Mat3 const localFromParent =
        Mat3::translate(pivot) * Mat3::rotate(rot) * Mat3::translate(Point{-pivot.x, -pivot.y});

    LayerNode group{};
    group.transform = localFromParent;
    group.opacity = 0.98f;
    group.clip = Rect::sharp(margin - 8.f, rowTop - 8.f, contentW + 16.f, rowH + 16.f);
    groupId_ = g.addLayer(std::move(group));

    auto cardR = [&](float x, float y, float w, float h, Color fill) {
      return RectNode{.bounds = Rect::sharp(x, y, w, h),
                     .cornerRadius = CornerRadius(14.f),
                     .fill = FillStyle::solid(fill),
                     .stroke = StrokeStyle::solid(Color::rgb(255, 255, 255), 1.6f)};
    };

    cardA_ = g.addRect(groupId_, cardR(rowOrigin.x, rowOrigin.y, cardW, rowH, Color::hex(0x3d5a80)));
    cardB_ = g.addRect(groupId_, cardR(rowOrigin.x + cardW + gap, rowOrigin.y, cardW, rowH, Color::hex(0x5a7d3a)));
    cardC_ = g.addRect(groupId_, cardR(rowOrigin.x + 2.f * (cardW + gap), rowOrigin.y, cardW, rowH, Color::hex(0x8a4a6a)));

    Point const c1{rowOrigin.x + cardW * 0.85f, rowOrigin.y + rowH * 0.72f};
    Point const c2{rowOrigin.x + cardW + gap + cardW * 0.15f, rowOrigin.y + rowH * 0.72f};
    LineNode line{};
    line.from = c1;
    line.to = c2;
    line.stroke = StrokeStyle::solid(Color::rgb(255, 255, 255), 3.f);
    lineId_ = g.addLine(groupId_, std::move(line));

    float const titleW = titleLayout->measuredSize.width;
    float const titleX = margin + (s.width - margin * 2.f - titleW) * 0.5f;
    titleId_ = g.addText(root, TextNode{.layout = std::move(titleLayout), .origin = Point{titleX, margin}});

    Font hintFont{.family = ".AppleSystemUIFont", .size = 14.f, .weight = 400.f};

    auto hintLayout =
        ts.layout("Layer, clip, transform, rects, line, text, hit-test", hintFont, Color::rgb(140, 150, 170),
                  s.width - margin * 2.f);
    float const hintW = hintLayout->measuredSize.width;
    float const hintX = margin + (s.width - margin * 2.f - hintW) * 0.5f;
    hintId_ = g.addText(root,
                        TextNode{.layout = std::move(hintLayout), .origin = Point{hintX, margin + titleBand + 6.f}});

    sceneBuilt_ = true;
    lastLayout_ = s;
  }

  void layoutScene(Size const& s) {
    SceneGraph& g = sceneGraph();

    float const margin = 28.f;
    TextSystem& ts = Application::instance().textSystem();
    Font titleFont{.family = ".AppleSystemUIFont", .size = 26.f, .weight = 600.f};
    float const titleBand =
        ts.layout("Scene graph", titleFont, Color::rgb(235, 238, 245), s.width - margin * 2.f)->measuredSize.height;
    float const rowTop = margin + titleBand + 18.f;
    float const rowH = 160.f;
    float const gap = 18.f;

    if (RectNode* bg = g.node<RectNode>(bgId_)) {
      bg->bounds = Rect::sharp(0.f, 0.f, s.width, s.height);
    }

    float const contentW = std::max(s.width - margin * 2.f, 120.f);
    float const cardW = (contentW - gap * 2.f) / 3.f;
    Point const rowOrigin{margin, rowTop};

    Point const pivot{s.width * 0.5f, rowTop + rowH * 0.5f};
    float const rot = 0.055f;
    if (LayerNode* group = g.node<LayerNode>(groupId_)) {
      group->transform =
          Mat3::translate(pivot) * Mat3::rotate(rot) * Mat3::translate(Point{-pivot.x, -pivot.y});
      group->clip = Rect::sharp(margin - 8.f, rowTop - 8.f, contentW + 16.f, rowH + 16.f);
    }

    if (RectNode* r = g.node<RectNode>(cardA_)) {
      r->bounds = Rect::sharp(rowOrigin.x, rowOrigin.y, cardW, rowH);
    }
    if (RectNode* r = g.node<RectNode>(cardB_)) {
      r->bounds = Rect::sharp(rowOrigin.x + cardW + gap, rowOrigin.y, cardW, rowH);
    }
    if (RectNode* r = g.node<RectNode>(cardC_)) {
      r->bounds = Rect::sharp(rowOrigin.x + 2.f * (cardW + gap), rowOrigin.y, cardW, rowH);
    }

    if (LineNode* ln = g.node<LineNode>(lineId_)) {
      ln->from = Point{rowOrigin.x + cardW * 0.85f, rowOrigin.y + rowH * 0.72f};
      ln->to = Point{rowOrigin.x + cardW + gap + cardW * 0.15f, rowOrigin.y + rowH * 0.72f};
    }

    if (TextNode* t = g.node<TextNode>(titleId_)) {
      t->layout = ts.layout("Scene graph", titleFont, Color::rgb(235, 238, 245), s.width - margin * 2.f);
      float const titleW = t->layout->measuredSize.width;
      t->origin = Point{margin + (s.width - margin * 2.f - titleW) * 0.5f, margin};
    }

    Font hintFont{.family = ".AppleSystemUIFont", .size = 14.f, .weight = 400.f};

    if (TextNode* t = g.node<TextNode>(hintId_)) {
      t->layout = ts.layout("Layer, clip, transform, rects, line, text, hit-test", hintFont,
                            Color::rgb(140, 150, 170), s.width - margin * 2.f);
      float const hintW = t->layout->measuredSize.width;
      t->origin = Point{margin + (s.width - margin * 2.f - hintW) * 0.5f, margin + titleBand + 6.f};
    }

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
      dumpSceneGraph(sceneGraph(), std::cerr);
    } else if (layout.width != lastLayout_.width || layout.height != lastLayout_.height) {
      layoutScene(layout);
      dumpSceneGraph(sceneGraph(), std::cerr);
    }

    SceneRenderer{}.render(sceneGraph(), canvas, Color::hex(0x1a1d24));
  }
};

} // namespace

int main(int argc, char* argv[]) {
  Application app(argc, argv);

  app.createWindow<SceneDemoWindow>({
      .size = {720, 480},
      .title = "Flux — Scene graph demo",
      .resizable = true,
  });

  return app.exec();
}
