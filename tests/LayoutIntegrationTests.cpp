#include <doctest/doctest.h>

#include <Flux/UI/Element.hpp>
#include <Flux/UI/LayoutContext.hpp>
#include <Flux/UI/LayoutEngine.hpp>
#include <Flux/UI/LayoutTree.hpp>
#include <Flux/UI/StateStore.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/Spacer.hpp>
#include <Flux/UI/Views/VStack.hpp>
#include <Flux/UI/Views/ZStack.hpp>

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
  std::shared_ptr<TextLayout> layout(AttributedString const&, float,
                                     TextLayoutOptions const&) override {
    return nullptr;
  }
  std::shared_ptr<TextLayout> layout(std::string_view, Font const&, Color const&, float,
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
