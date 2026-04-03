#include <doctest/doctest.h>

#include <Flux/UI/LayoutTree.hpp>

#include <cmath>

using namespace flux;

// ── Helpers ─────────────────────────────────────────────────────────────────

static LayoutNode makeNode(LayoutNode::Kind kind = LayoutNode::Kind::Leaf) {
  LayoutNode n{};
  n.kind = kind;
  return n;
}

static bool rectsNear(Rect const& a, Rect const& b, float eps = 1e-4f) {
  return std::fabs(a.x - b.x) < eps && std::fabs(a.y - b.y) < eps &&
         std::fabs(a.width - b.width) < eps && std::fabs(a.height - b.height) < eps;
}

// ── LayoutNodeId ─────────────────────────────────────────────────────────────

TEST_CASE("LayoutNodeId: default is invalid") {
  LayoutNodeId id{};
  CHECK_FALSE(id.isValid());
  CHECK(id.value == 0);
}

TEST_CASE("LayoutNodeId: fromIndex round-trips") {
  for (std::size_t i = 0; i < 5; ++i) {
    LayoutNodeId id = LayoutNodeId::fromIndex(i);
    CHECK(id.isValid());
    CHECK(id.index() == i);
  }
}

TEST_CASE("LayoutNodeId: equality") {
  LayoutNodeId a = LayoutNodeId::fromIndex(0);
  LayoutNodeId b = LayoutNodeId::fromIndex(0);
  LayoutNodeId c = LayoutNodeId::fromIndex(1);
  CHECK(a == b);
  CHECK(a != c);
}

// ── LayoutTree: basic push ──────────────────────────────────────────────────

TEST_CASE("LayoutTree: empty tree has invalid root") {
  LayoutTree tree;
  CHECK_FALSE(tree.root().isValid());
  CHECK(tree.nodes().empty());
}

TEST_CASE("LayoutTree: push single root node") {
  LayoutTree tree;
  LayoutNode n = makeNode();
  n.frame = Rect{10.f, 20.f, 100.f, 50.f};

  LayoutNodeId id = tree.pushNode(std::move(n), LayoutNodeId{});

  CHECK(id.isValid());
  CHECK(tree.root() == id);
  CHECK(tree.nodes().size() == 1);

  LayoutNode const* stored = tree.get(id);
  REQUIRE(stored != nullptr);
  CHECK(stored->id == id);
  CHECK_FALSE(stored->parent.isValid());
  CHECK(rectsNear(stored->frame, Rect{10.f, 20.f, 100.f, 50.f}));
}

TEST_CASE("LayoutTree: push parent and two children") {
  LayoutTree tree;

  LayoutNode parent = makeNode(LayoutNode::Kind::Container);
  parent.frame = Rect{0.f, 0.f, 200.f, 400.f};
  LayoutNodeId parentId = tree.pushNode(std::move(parent), LayoutNodeId{});

  LayoutNode child1 = makeNode();
  child1.frame = Rect{0.f, 0.f, 200.f, 100.f};
  LayoutNodeId c1Id = tree.pushNode(std::move(child1), parentId);

  LayoutNode child2 = makeNode();
  child2.frame = Rect{0.f, 108.f, 200.f, 100.f};
  LayoutNodeId c2Id = tree.pushNode(std::move(child2), parentId);

  CHECK(tree.nodes().size() == 3);
  CHECK(tree.root() == parentId);

  LayoutNode const* p = tree.get(parentId);
  REQUIRE(p != nullptr);
  CHECK(p->children.size() == 2);
  CHECK(p->children[0] == c1Id);
  CHECK(p->children[1] == c2Id);

  LayoutNode const* c1 = tree.get(c1Id);
  REQUIRE(c1 != nullptr);
  CHECK(c1->parent == parentId);
  CHECK(rectsNear(c1->frame, Rect{0.f, 0.f, 200.f, 100.f}));

  LayoutNode const* c2 = tree.get(c2Id);
  REQUIRE(c2 != nullptr);
  CHECK(c2->parent == parentId);
  CHECK(rectsNear(c2->frame, Rect{0.f, 108.f, 200.f, 100.f}));
}

TEST_CASE("LayoutTree: nodes() span matches insertion order") {
  LayoutTree tree;
  LayoutNodeId rootId = tree.pushNode(makeNode(LayoutNode::Kind::Container), LayoutNodeId{});
  LayoutNodeId c1Id = tree.pushNode(makeNode(), rootId);
  LayoutNodeId c2Id = tree.pushNode(makeNode(), rootId);

  auto span = tree.nodes();
  REQUIRE(span.size() == 3);
  CHECK(span[0].id == rootId);
  CHECK(span[1].id == c1Id);
  CHECK(span[2].id == c2Id);
}

TEST_CASE("LayoutTree: get() returns nullptr for invalid id") {
  LayoutTree tree;
  CHECK(tree.get(LayoutNodeId{}) == nullptr);
}

TEST_CASE("LayoutTree: get() returns nullptr for out-of-range id") {
  LayoutTree tree;
  tree.pushNode(makeNode(), LayoutNodeId{});
  LayoutNodeId outOfRange = LayoutNodeId::fromIndex(99);
  CHECK(tree.get(outOfRange) == nullptr);
}

TEST_CASE("LayoutTree: clear() resets tree") {
  LayoutTree tree;
  tree.pushNode(makeNode(), LayoutNodeId{});
  tree.pushNode(makeNode(), LayoutNodeId::fromIndex(0));

  tree.clear();

  CHECK_FALSE(tree.root().isValid());
  CHECK(tree.nodes().empty());
}

// ── LayoutTree: rectForKey ───────────────────────────────────────────────────

TEST_CASE("LayoutTree: rectForKey returns nullopt for empty tree") {
  LayoutTree tree;
  ComponentKey key = {0, 1};
  CHECK_FALSE(tree.rectForKey(key).has_value());
}

TEST_CASE("LayoutTree: rectForKey finds node by component key") {
  LayoutTree tree;

  LayoutNode n = makeNode();
  n.worldBounds = Rect{5.f, 10.f, 80.f, 40.f};
  n.componentKey = ComponentKey{0, 3};
  tree.pushNode(std::move(n), LayoutNodeId{});

  auto result = tree.rectForKey(ComponentKey{0, 3});
  REQUIRE(result.has_value());
  CHECK(rectsNear(*result, Rect{5.f, 10.f, 80.f, 40.f}));
}

TEST_CASE("LayoutTree: rectForKey returns first matching node") {
  LayoutTree tree;

  LayoutNode parent = makeNode(LayoutNode::Kind::Container);
  parent.worldBounds = Rect{0.f, 0.f, 200.f, 200.f};
  parent.componentKey = ComponentKey{1};
  LayoutNodeId parentId = tree.pushNode(std::move(parent), LayoutNodeId{});

  LayoutNode child = makeNode();
  child.worldBounds = Rect{10.f, 10.f, 50.f, 50.f};
  child.componentKey = ComponentKey{2};
  tree.pushNode(std::move(child), parentId);

  auto r1 = tree.rectForKey(ComponentKey{1});
  REQUIRE(r1.has_value());
  CHECK(rectsNear(*r1, Rect{0.f, 0.f, 200.f, 200.f}));

  auto r2 = tree.rectForKey(ComponentKey{2});
  REQUIRE(r2.has_value());
  CHECK(rectsNear(*r2, Rect{10.f, 10.f, 50.f, 50.f}));

  auto missing = tree.rectForKey(ComponentKey{99});
  CHECK_FALSE(missing.has_value());
}

// ── LayoutTree: unionSubtreeWorldBounds ─────────────────────────────────────

TEST_CASE("LayoutTree: unionSubtreeWorldBounds for single node") {
  LayoutTree tree;
  LayoutNode n = makeNode();
  n.worldBounds = Rect{10.f, 20.f, 100.f, 50.f};
  LayoutNodeId id = tree.pushNode(std::move(n), LayoutNodeId{});

  Rect r = tree.unionSubtreeWorldBounds(id);
  CHECK(rectsNear(r, Rect{10.f, 20.f, 100.f, 50.f}));
}

TEST_CASE("LayoutTree: unionSubtreeWorldBounds unions children") {
  LayoutTree tree;

  LayoutNode parent = makeNode(LayoutNode::Kind::Container);
  parent.worldBounds = Rect{0.f, 0.f, 200.f, 200.f};
  LayoutNodeId parentId = tree.pushNode(std::move(parent), LayoutNodeId{});

  LayoutNode c1 = makeNode();
  c1.worldBounds = Rect{10.f, 10.f, 80.f, 40.f};
  tree.pushNode(std::move(c1), parentId);

  LayoutNode c2 = makeNode();
  c2.worldBounds = Rect{50.f, 100.f, 100.f, 60.f};
  tree.pushNode(std::move(c2), parentId);

  // Union of parent (0,0,200,200), c1 (10,10,80,40 → x2=90,y2=50),
  // c2 (50,100,100,60 → x2=150,y2=160): overall (0,0,200,200)
  Rect r = tree.unionSubtreeWorldBounds(parentId);
  CHECK(rectsNear(r, Rect{0.f, 0.f, 200.f, 200.f}));
}

TEST_CASE("LayoutTree: unionSubtreeWorldBounds returns empty for invalid id") {
  LayoutTree tree;
  tree.pushNode(makeNode(), LayoutNodeId{});

  Rect r = tree.unionSubtreeWorldBounds(LayoutNodeId{});
  CHECK(rectsNear(r, Rect{0.f, 0.f, 0.f, 0.f}));
}

// ── transformWorldBounds ─────────────────────────────────────────────────────

TEST_CASE("transformWorldBounds: identity transform preserves rect") {
  Rect r{10.f, 20.f, 100.f, 50.f};
  Rect t = transformWorldBounds(Mat3::identity(), r);
  CHECK(rectsNear(t, r));
}

TEST_CASE("transformWorldBounds: translation offsets bounds") {
  Rect r{0.f, 0.f, 100.f, 50.f};
  Rect t = transformWorldBounds(Mat3::translate(30.f, 15.f), r);
  CHECK(rectsNear(t, Rect{30.f, 15.f, 100.f, 50.f}));
}

TEST_CASE("transformWorldBounds: nested translation accumulates") {
  // Simulate a layer world transform of translate(10, 20) * translate(5, 8)
  Mat3 combined = Mat3::translate(10.f, 20.f) * Mat3::translate(5.f, 8.f);
  Rect r{0.f, 0.f, 100.f, 40.f};
  Rect t = transformWorldBounds(combined, r);
  CHECK(rectsNear(t, Rect{15.f, 28.f, 100.f, 40.f}));
}
