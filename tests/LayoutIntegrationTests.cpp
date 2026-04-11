#include <doctest/doctest.h>

#include <Flux/UI/Element.hpp>
#include <Flux/UI/LayoutContext.hpp>
#include <Flux/UI/LayoutEngine.hpp>
#include <Flux/UI/LayoutTree.hpp>
#include <Flux/UI/RenderContext.hpp>
#include <Flux/UI/StateStore.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Environment.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/Grid.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/Spacer.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/VStack.hpp>
#include <Flux/UI/Views/ZStack.hpp>

#include <Flux/Scene/SceneGraph.hpp>
#include <Flux/Scene/SceneGraphBounds.hpp>

#include <Flux/UI/EventMap.hpp>
#include <Flux/Graphics/TextSystem.hpp>

#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

using namespace flux;

// ── Minimal TextSystem stub ───────────────────────────────────────────────────

class NullTextSystem final : public TextSystem {
public:
  std::shared_ptr<TextLayout const> layout(AttributedString const&, float,
                                           TextLayoutOptions const&) override {
    return nullptr;
  }
  std::shared_ptr<TextLayout const> layout(std::string_view, Font const&, Color const&, float,
                                           TextLayoutOptions const&) override {
    return nullptr;
  }
  Size measure(AttributedString const&, float, TextLayoutOptions const&) override { return {}; }
  Size measure(std::string_view, Font const&, Color const&, float,
               TextLayoutOptions const&) override {
    return {};
  }
  std::uint32_t resolveFontId(std::string_view, float, bool) override { return 0; }
  std::vector<std::uint8_t> rasterizeGlyph(std::uint32_t, std::uint16_t, float,
                                            std::uint32_t&, std::uint32_t&,
                                            Point&) override {
    return {};
  }
};

class RecordingTextSystem final : public TextSystem {
public:
  Font lastMeasureFont{};
  Color lastMeasureColor{};
  Font lastLayoutFont{};
  Color lastLayoutColor{};
  bool measured = false;
  bool laidOut = false;

  std::shared_ptr<TextLayout const> layout(AttributedString const&, float,
                                           TextLayoutOptions const&) override {
    return nullptr;
  }

  std::shared_ptr<TextLayout const> layout(std::string_view, Font const& font, Color const& color, float,
                                           TextLayoutOptions const&) override {
    laidOut = true;
    lastLayoutFont = font;
    lastLayoutColor = color;

    auto layout = std::make_shared<TextLayout>();
    layout->ownedStorage = std::make_unique<TextLayoutStorage>();
    layout->ownedStorage->glyphArena = {1};
    layout->ownedStorage->positionArena = {{0.f, 0.f}};

    TextLayout::PlacedRun run{};
    run.run.fontId = 1;
    run.run.fontSize = font.size;
    run.run.color = color;
    run.run.glyphIds = std::span<std::uint16_t const>(layout->ownedStorage->glyphArena.data(), 1);
    run.run.positions = std::span<Point const>(layout->ownedStorage->positionArena.data(), 1);
    run.run.ascent = 8.f;
    run.run.descent = 2.f;
    run.run.width = 10.f;
    run.origin = {0.f, 8.f};
    layout->runs.push_back(run);
    recomputeTextLayoutMetrics(*layout);
    return layout;
  }

  Size measure(AttributedString const&, float, TextLayoutOptions const&) override { return {}; }

  Size measure(std::string_view, Font const& font, Color const& color, float,
               TextLayoutOptions const&) override {
    measured = true;
    lastMeasureFont = font;
    lastMeasureColor = color;
    return {10.f, 10.f};
  }

  std::uint32_t resolveFontId(std::string_view, float, bool) override { return 0; }

  std::vector<std::uint8_t> rasterizeGlyph(std::uint32_t, std::uint16_t, float, std::uint32_t&,
                                           std::uint32_t&, Point&) override {
    return {};
  }
};

class LineOnlyLayoutTextSystem final : public TextSystem {
public:
  std::shared_ptr<TextLayout const> layout(AttributedString const&, float,
                                           TextLayoutOptions const&) override {
    return nullptr;
  }

  std::shared_ptr<TextLayout const> layout(std::string_view, Font const&, Color const&, float,
                                           TextLayoutOptions const&) override {
    auto layout = std::make_shared<TextLayout>();
    layout->lines.push_back(TextLayout::LineRange{
        .ctLineIndex = 0,
        .byteStart = 0,
        .byteEnd = 1,
        .lineMinX = 0.f,
        .top = 0.f,
        .bottom = 12.f,
        .baseline = 9.f,
    });
    layout->measuredSize = {0.f, 12.f};
    layout->firstBaseline = 9.f;
    layout->lastBaseline = 9.f;
    return layout;
  }

  Size measure(AttributedString const&, float, TextLayoutOptions const&) override { return {}; }
  Size measure(std::string_view, Font const&, Color const&, float, TextLayoutOptions const&) override {
    return {};
  }
  std::uint32_t resolveFontId(std::string_view, float, bool) override { return 0; }
  std::vector<std::uint8_t> rasterizeGlyph(std::uint32_t, std::uint16_t, float, std::uint32_t&,
                                           std::uint32_t&, Point&) override {
    return {};
  }
};

// ── Test-accessible LayoutContext factory ──────────────────────────────────────
// LayoutContextTestAccess is declared as a friend in LayoutContext.hpp.

namespace flux {
struct LayoutContextTestAccess {
  static LayoutContext* create(TextSystem& ts, LayoutEngine& le, LayoutTree& tree) {
    return new LayoutContext(ts, le, tree, nullptr);
  }
  static void destroy(LayoutContext* ctx) { delete ctx; }
};
} // namespace flux

struct LayoutContextDeleter {
  void operator()(flux::LayoutContext* p) const { flux::LayoutContextTestAccess::destroy(p); }
};
using LayoutContextPtr = std::unique_ptr<flux::LayoutContext, LayoutContextDeleter>;

struct EnvironmentGuard {
  explicit EnvironmentGuard(EnvironmentLayer layer) { EnvironmentStack::current().push(std::move(layer)); }
  ~EnvironmentGuard() { EnvironmentStack::current().pop(); }
};

// ── Helpers ───────────────────────────────────────────────────────────────────

static bool rectsNear(Rect const& a, Rect const& b, float eps = 0.5f) {
  return std::fabs(a.x - b.x) < eps && std::fabs(a.y - b.y) < eps &&
         std::fabs(a.width - b.width) < eps && std::fabs(a.height - b.height) < eps;
}

// Lay out a single root element at the given constraints and return the LayoutTree.
// No SceneGraph, no EventMap, no RenderContext involved.
static LayoutTree runLayout(Element el, float maxW, float maxH) {
  NullTextSystem ts;
  LayoutEngine le;
  LayoutTree tree;
  le.resetForBuild();

  LayoutContextPtr ctx{flux::LayoutContextTestAccess::create(ts, le, tree)};

  LayoutConstraints rootCs{};
  rootCs.maxWidth = maxW;
  rootCs.maxHeight = maxH;
  ctx->pushConstraints(rootCs);
  le.setChildFrame(Rect{0.f, 0.f, maxW, maxH});

  StateStore::setCurrent(nullptr);
  el.layout(*ctx);

  ctx->popConstraints();
  return tree;
}

static std::vector<LayoutNode const*> leavesOf(LayoutTree const& tree) {
  std::vector<LayoutNode const*> out;
  for (auto const& n : tree.nodes()) {
    if (n.kind == LayoutNode::Kind::Leaf) {
      out.push_back(&n);
    }
  }
  return out;
}

static LayoutContextPtr makeLayoutContext(TextSystem& ts, LayoutEngine& le, LayoutTree& tree, float maxW,
                                          float maxH) {
  le.resetForBuild();
  LayoutContextPtr ctx{flux::LayoutContextTestAccess::create(ts, le, tree)};
  LayoutConstraints rootCs{};
  rootCs.maxWidth = maxW;
  rootCs.maxHeight = maxH;
  ctx->pushConstraints(rootCs);
  le.setChildFrame(Rect{0.f, 0.f, maxW, maxH});
  return ctx;
}

// ── VStack ────────────────────────────────────────────────────────────────────

TEST_CASE("VStack: 3 fixed-height rectangles stacked with no spacing") {
  auto tree = runLayout(
      Element{VStack{
          .spacing = 0.f,
          .children = children(
              Element{Rectangle{}}.size(200.f, 50.f),
              Element{Rectangle{}}.size(200.f, 50.f),
              Element{Rectangle{}}.size(200.f, 50.f)),
      }},
      200.f, 300.f);

  auto leaves = leavesOf(tree);
  REQUIRE(leaves.size() == 3);
  CHECK(rectsNear(leaves[0]->frame, Rect{0.f, 0.f, 200.f, 50.f}));
  CHECK(rectsNear(leaves[1]->frame, Rect{0.f, 50.f, 200.f, 50.f}));
  CHECK(rectsNear(leaves[2]->frame, Rect{0.f, 100.f, 200.f, 50.f}));
}

TEST_CASE("VStack: spacing is applied between children") {
  const float spacing = 8.f;
  auto tree = runLayout(
      Element{VStack{
          .spacing = spacing,
          .children = children(
              Element{Rectangle{}}.size(100.f, 30.f),
              Element{Rectangle{}}.size(100.f, 30.f)),
      }},
      200.f, 300.f);

  auto leaves = leavesOf(tree);
  REQUIRE(leaves.size() == 2);
  CHECK(rectsNear(leaves[0]->frame, Rect{0.f, 0.f, 100.f, 30.f}));
  CHECK(rectsNear(leaves[1]->frame, Rect{0.f, 38.f, 100.f, 30.f}));
}

TEST_CASE("VStack: flex grow fills remaining vertical space") {
  // Total height 200 = 50 (fixed) + flex + 50 (fixed)
  auto tree = runLayout(
      Element{VStack{
          .spacing = 0.f,
          .children = children(
              Element{Rectangle{}}.size(100.f, 50.f),
              Element{Rectangle{}}.flex(1.f, 0.f, 0.f),
              Element{Rectangle{}}.size(100.f, 50.f)),
      }},
      100.f, 200.f);

  auto leaves = leavesOf(tree);
  REQUIRE(leaves.size() == 3);
  CHECK(rectsNear(leaves[0]->frame, Rect{0.f, 0.f, 100.f, 50.f}));
  CHECK(rectsNear(leaves[1]->frame, Rect{0.f, 50.f, 100.f, 100.f}));
  CHECK(rectsNear(leaves[2]->frame, Rect{0.f, 150.f, 100.f, 50.f}));
}

// ── HStack ────────────────────────────────────────────────────────────────────

TEST_CASE("HStack: 3 fixed-width rectangles side by side with no spacing") {
  auto tree = runLayout(
      Element{HStack{
          .spacing = 0.f,
          .children = children(
              Element{Rectangle{}}.size(60.f, 50.f),
              Element{Rectangle{}}.size(60.f, 50.f),
              Element{Rectangle{}}.size(60.f, 50.f)),
      }},
      300.f, 100.f);

  auto leaves = leavesOf(tree);
  REQUIRE(leaves.size() == 3);
  CHECK(rectsNear(leaves[0]->frame, Rect{0.f, 0.f, 60.f, 50.f}));
  CHECK(rectsNear(leaves[1]->frame, Rect{60.f, 0.f, 60.f, 50.f}));
  CHECK(rectsNear(leaves[2]->frame, Rect{120.f, 0.f, 60.f, 50.f}));
}

TEST_CASE("HStack: stretch cross-axis expands explicit-height children to row height") {
  auto tree = runLayout(
      Element{HStack{
          .spacing = 0.f,
          .alignment = Alignment::Stretch,
          .children = children(
              Element{Rectangle{}}.size(40.f, 40.f),
              Element{Rectangle{}}.size(40.f, 100.f)),
      }},
      200.f, 200.f);

  auto leaves = leavesOf(tree);
  REQUIRE(leaves.size() == 2);
  // Row height is the max of intrinsic child heights; stretch gives each child that full height.
  CHECK(rectsNear(leaves[0]->frame, Rect{0.f, 0.f, 40.f, 200.f}));
  CHECK(rectsNear(leaves[1]->frame, Rect{40.f, 0.f, 40.f, 200.f}));
}

TEST_CASE("HStack: measure reflects flexed widths under finite constraint") {
  NullTextSystem ts;
  LayoutEngine le;
  LayoutTree tree;
  LayoutContextPtr ctx = makeLayoutContext(ts, le, tree, 200.f, 100.f);

  Element el{HStack{
      .spacing = 0.f,
      .children = children(
          Element{Rectangle{}}.size(40.f, 20.f),
          Element{Rectangle{}}.flex(1.f, 1.f, 0.f),
          Element{Rectangle{}}.size(40.f, 20.f)),
  }};

  Size const measured = el.measure(*ctx, LayoutConstraints{.maxWidth = 200.f, .maxHeight = 100.f}, {}, ts);
  CHECK(measured.width == doctest::Approx(200.f));
}

TEST_CASE("Text measure resolves theme font and color before TextSystem") {
  RecordingTextSystem ts;
  LayoutEngine le;
  LayoutTree tree;
  LayoutContextPtr ctx = makeLayoutContext(ts, le, tree, 200.f, 100.f);

  Theme theme = Theme::light();
  theme.fontBody = Font{.family = "Theme Body", .size = 19.f, .weight = 510.f};
  theme.colorTextPrimary = Color::hex(0x123456);
  EnvironmentLayer layer;
  layer.set<Theme>(theme);
  EnvironmentGuard guard(std::move(layer));

  Text text{.text = "hello"};
  Size const measured = text.measure(*ctx, LayoutConstraints{.maxWidth = 200.f, .maxHeight = 100.f}, {}, ts);

  CHECK(ts.measured);
  CHECK(measured.width == doctest::Approx(10.f));
  CHECK(measured.height == doctest::Approx(10.f));
  CHECK(ts.lastMeasureFont.family == "Theme Body");
  CHECK(ts.lastMeasureFont.size == doctest::Approx(19.f));
  CHECK(ts.lastMeasureFont.weight == doctest::Approx(510.f));
  CHECK(ts.lastMeasureColor == Color::hex(0x123456));

  ctx->popConstraints();
}

TEST_CASE("Text render resolves theme font and color before TextSystem") {
  RecordingTextSystem ts;
  SceneGraph graph;
  EventMap eventMap;
  RenderContext rctx{graph, eventMap, ts};

  Theme theme = Theme::light();
  theme.fontBody = Font{.family = "Render Theme", .size = 17.f, .weight = 480.f};
  theme.colorTextPrimary = Color::hex(0xABCDEF);
  EnvironmentLayer layer;
  layer.set<Theme>(theme);
  EnvironmentGuard guard(std::move(layer));

  LayoutConstraints cs{};
  cs.maxWidth = 200.f;
  cs.maxHeight = 50.f;
  rctx.pushConstraints(cs, {});

  Text text{.text = "icon-like"};
  LayoutNode node{};
  node.frame = Rect{0.f, 0.f, 100.f, 20.f};
  text.renderFromLayout(rctx, node);

  CHECK(ts.laidOut);
  CHECK(ts.lastLayoutFont.family == "Render Theme");
  CHECK(ts.lastLayoutFont.size == doctest::Approx(17.f));
  CHECK(ts.lastLayoutFont.weight == doctest::Approx(480.f));
  CHECK(ts.lastLayoutColor == Color::hex(0xABCDEF));

  rctx.popConstraints();
}

TEST_CASE("trimTextLayoutToMaxLines keeps all runs from the first ctLineIndex") {
  TextLayout layout;

  auto storage = std::make_unique<TextLayoutStorage>();
  storage->glyphArena = {1, 2, 3};
  storage->positionArena = {{0.f, 0.f}, {8.f, 0.f}, {0.f, 0.f}};

  TextLayout::PlacedRun runA{};
  runA.run.glyphIds = std::span<std::uint16_t const>(storage->glyphArena.data(), 1);
  runA.run.positions = std::span<Point const>(storage->positionArena.data(), 1);
  runA.run.ascent = 8.f;
  runA.run.descent = 2.f;
  runA.run.width = 8.f;
  runA.origin = {0.f, 10.f};
  runA.ctLineIndex = 0;

  TextLayout::PlacedRun runB = runA;
  runB.run.glyphIds = std::span<std::uint16_t const>(storage->glyphArena.data() + 1, 1);
  runB.run.positions = std::span<Point const>(storage->positionArena.data() + 1, 1);
  runB.origin = {8.f, 12.f};
  runB.ctLineIndex = 0;

  TextLayout::PlacedRun runC = runA;
  runC.run.glyphIds = std::span<std::uint16_t const>(storage->glyphArena.data() + 2, 1);
  runC.run.positions = std::span<Point const>(storage->positionArena.data() + 2, 1);
  runC.origin = {0.f, 30.f};
  runC.ctLineIndex = 1;

  layout.runs = {runA, runB, runC};
  layout.lines = {
      TextLayout::LineRange{.ctLineIndex = 0, .top = 2.f, .bottom = 14.f, .baseline = 10.f},
      TextLayout::LineRange{.ctLineIndex = 1, .top = 22.f, .bottom = 34.f, .baseline = 30.f},
  };
  layout.ownedStorage = std::move(storage);
  recomputeTextLayoutMetrics(layout);

  trimTextLayoutToMaxLines(layout, 1, false);

  REQUIRE(layout.runs.size() == 2);
  CHECK(layout.runs[0].ctLineIndex == 0);
  CHECK(layout.runs[1].ctLineIndex == 0);
  REQUIRE(layout.lines.size() == 1);
  CHECK(layout.lines[0].ctLineIndex == 0);
}

TEST_CASE("Text render emits a TextNode for line-only layouts") {
  LineOnlyLayoutTextSystem ts;
  SceneGraph graph;
  EventMap eventMap;
  RenderContext rctx{graph, eventMap, ts};

  LayoutConstraints cs{};
  cs.maxWidth = 200.f;
  cs.maxHeight = 50.f;
  rctx.pushConstraints(cs, {});

  Text text{.text = "\n"};
  LayoutNode node{};
  node.frame = Rect{5.f, 7.f, 100.f, 20.f};
  text.renderFromLayout(rctx, node);

  auto const* root = graph.node<LayerNode>(graph.root());
  REQUIRE(root != nullptr);
  REQUIRE(root->children.size() == 1);
  auto const* textNode = graph.node<TextNode>(root->children[0]);
  REQUIRE(textNode != nullptr);
  CHECK(textNode->layout != nullptr);
  CHECK(textNode->layout->runs.empty());
  CHECK(textNode->layout->lines.size() == 1);

  Rect const bounds = measureRootContentBounds(graph);
  CHECK(bounds.x == doctest::Approx(5.f));
  CHECK(bounds.y == doctest::Approx(7.f));
  CHECK(bounds.width == doctest::Approx(100.f));
  CHECK(bounds.height == doctest::Approx(20.f));

  rctx.popConstraints();
}

TEST_CASE("VStack: stretch cross-axis expands explicit-width children to column width") {
  auto tree = runLayout(
      Element{VStack{
          .spacing = 0.f,
          .alignment = Alignment::Stretch,
          .children = children(
              Element{Rectangle{}}.size(80.f, 30.f),
              Element{Rectangle{}}.size(200.f, 30.f)),
      }},
      200.f, 300.f);

  auto leaves = leavesOf(tree);
  REQUIRE(leaves.size() == 2);
  CHECK(rectsNear(leaves[0]->frame, Rect{0.f, 0.f, 200.f, 30.f}));
  CHECK(rectsNear(leaves[1]->frame, Rect{0.f, 30.f, 200.f, 30.f}));
}

TEST_CASE("VStack: measure reflects flexed heights under finite constraint") {
  NullTextSystem ts;
  LayoutEngine le;
  LayoutTree tree;
  LayoutContextPtr ctx = makeLayoutContext(ts, le, tree, 100.f, 200.f);

  Element el{VStack{
      .spacing = 0.f,
      .children = children(
          Element{Rectangle{}}.size(100.f, 40.f),
          Element{Rectangle{}}.flex(1.f, 1.f, 0.f),
          Element{Rectangle{}}.size(100.f, 40.f)),
  }};

  Size const measured = el.measure(*ctx, LayoutConstraints{.maxWidth = 100.f, .maxHeight = 200.f}, {}, ts);
  CHECK(measured.height == doctest::Approx(200.f));
}

// ── Grid ──────────────────────────────────────────────────────────────────────

TEST_CASE("Grid: unconstrained width uses intrinsic column widths without overlap") {
  auto tree = runLayout(
      Element{Grid{
          .columns = 2,
          .horizontalSpacing = 10.f,
          .verticalSpacing = 12.f,
          .horizontalAlignment = Alignment::Start,
          .verticalAlignment = Alignment::Start,
          .children = children(
              Element{Rectangle{}}.size(80.f, 30.f),
              Element{Rectangle{}}.size(50.f, 30.f),
              Element{Rectangle{}}.size(60.f, 40.f),
              Element{Rectangle{}}.size(40.f, 40.f)),
      }},
      0.f, 0.f);

  auto leaves = leavesOf(tree);
  REQUIRE(leaves.size() == 4);
  CHECK(rectsNear(leaves[0]->frame, Rect{0.f, 0.f, 80.f, 30.f}));
  CHECK(rectsNear(leaves[1]->frame, Rect{90.f, 0.f, 50.f, 30.f}));
  CHECK(rectsNear(leaves[2]->frame, Rect{0.f, 42.f, 60.f, 40.f}));
  CHECK(rectsNear(leaves[3]->frame, Rect{90.f, 42.f, 40.f, 40.f}));
}

TEST_CASE("Grid: measure reports intrinsic width when width is unconstrained") {
  NullTextSystem ts;
  LayoutEngine le;
  LayoutTree tree;
  LayoutContextPtr ctx = makeLayoutContext(ts, le, tree, 0.f, 0.f);

  Element el{Grid{
      .columns = 2,
      .horizontalSpacing = 10.f,
      .verticalSpacing = 12.f,
      .children = children(
          Element{Rectangle{}}.size(80.f, 30.f),
          Element{Rectangle{}}.size(50.f, 30.f),
          Element{Rectangle{}}.size(60.f, 40.f),
          Element{Rectangle{}}.size(40.f, 40.f)),
  }};

  Size const measured = el.measure(*ctx, LayoutConstraints{}, {}, ts);
  CHECK(measured.width == doctest::Approx(140.f));
  CHECK(measured.height == doctest::Approx(82.f));
}

// ── ZStack ────────────────────────────────────────────────────────────────────

TEST_CASE("ZStack: children share the same layer and the smaller is centered") {
  auto tree = runLayout(
      Element{ZStack{
          .children = children(
              Element{Rectangle{}}.size(200.f, 200.f),
              Element{Rectangle{}}.size(100.f, 100.f)),
      }},
      200.f, 200.f);

  auto leaves = leavesOf(tree);
  REQUIRE(leaves.size() == 2);
  CHECK(rectsNear(leaves[0]->frame, Rect{0.f, 0.f, 200.f, 200.f}));
  // Centered: (200-100)/2 = 50 on both axes
  CHECK(rectsNear(leaves[1]->frame, Rect{50.f, 50.f, 100.f, 100.f}));
}

// ── Padding modifier ──────────────────────────────────────────────────────────

TEST_CASE("Padding modifier insets inner content frame") {
  // A Rectangle with .size(80, 80).padding(10) inside a VStack:
  //   - outer measured size: 80 + 2*10 = 100 x ? (Rectangle height 0 + pad*2 = 20)
  //   - modifier frame is what VStack assigns
  //   - leaf frame (inner Rectangle) is inset by padding
  const float pad = 10.f;
  auto tree = runLayout(
      Element{VStack{
          .spacing = 0.f,
          .children = children(
              Element{Rectangle{}}.size(80.f, 80.f).padding(pad)),
      }},
      200.f, 200.f);

  LayoutNode const* modifierNode = nullptr;
  LayoutNode const* leafNode = nullptr;
  for (auto const& n : tree.nodes()) {
    if (n.kind == LayoutNode::Kind::Modifier) modifierNode = &n;
    else if (n.kind == LayoutNode::Kind::Leaf) leafNode = &n;
  }
  REQUIRE(modifierNode != nullptr);
  REQUIRE(leafNode != nullptr);

  // Modifier frame is assigned by VStack at (0,0); the sizeWidth/Height on the modifier
  // is respected during measure (returns 100x100) so VStack assigns {0,0,200,100} (full width).
  // The modifier's own bgBounds = absOuter = parentFrame = {0,0,200,100}
  CHECK(modifierNode->frame.x == doctest::Approx(0.f).epsilon(0.5f));
  CHECK(modifierNode->frame.y == doctest::Approx(0.f).epsilon(0.5f));

  // Leaf is inset by pad from the modifier frame origin
  CHECK(leafNode->frame.x == doctest::Approx(modifierNode->frame.x + pad).epsilon(0.5f));
  CHECK(leafNode->frame.y == doctest::Approx(modifierNode->frame.y + pad).epsilon(0.5f));
  CHECK(leafNode->frame.width == doctest::Approx(80.f).epsilon(0.5f));
  CHECK(leafNode->frame.height == doctest::Approx(80.f).epsilon(0.5f));
}

// ── Nested containers ─────────────────────────────────────────────────────────

TEST_CASE("VStack containing HStack: leaf frames are in HStack layer space") {
  auto tree = runLayout(
      Element{VStack{
          .spacing = 0.f,
          .children = children(
              Element{HStack{
                  .spacing = 0.f,
                  .children = children(
                      Element{Rectangle{}}.size(60.f, 40.f),
                      Element{Rectangle{}}.size(60.f, 40.f),
                      Element{Rectangle{}}.size(60.f, 40.f)),
              }}),
      }},
      300.f, 200.f);

  auto leaves = leavesOf(tree);
  REQUIRE(leaves.size() == 3);
  CHECK(rectsNear(leaves[0]->frame, Rect{0.f, 0.f, 60.f, 40.f}));
  CHECK(rectsNear(leaves[1]->frame, Rect{60.f, 0.f, 60.f, 40.f}));
  CHECK(rectsNear(leaves[2]->frame, Rect{120.f, 0.f, 60.f, 40.f}));
}
