#include <doctest/doctest.h>

#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/Reactive/Computed.hpp>
#include <Flux/Reactive/Signal.hpp>
#include <Flux/Scene/SceneGraph.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/EventMap.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/LayoutContext.hpp>
#include <Flux/UI/LayoutEngine.hpp>
#include <Flux/UI/LayoutTree.hpp>
#include <Flux/UI/MeasureCache.hpp>
#include <Flux/UI/RenderContext.hpp>
#include <Flux/UI/RenderLayoutTree.hpp>
#include <Flux/UI/StateStore.hpp>
#include <Flux/UI/Views/ForEach.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/Image.hpp>
#include <Flux/UI/Views/Line.hpp>
#include <Flux/UI/Views/PathShape.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/VStack.hpp>

#include <cstdint>
#include <algorithm>
#include <memory>
#include <string_view>
#include <vector>

namespace {

using namespace flux;

class NullTextSystem final : public TextSystem {
public:
  std::shared_ptr<TextLayout const> layout(AttributedString const&, float,
                                           TextLayoutOptions const&) override {
    auto out = std::make_shared<TextLayout>();
    out->measuredSize = Size{48.f, 14.f};
    return out;
  }

  std::shared_ptr<TextLayout const> layout(std::string_view, Font const&, Color const&, float,
                                           TextLayoutOptions const&) override {
    auto out = std::make_shared<TextLayout>();
    out->measuredSize = Size{48.f, 14.f};
    return out;
  }

  Size measure(AttributedString const& text, float, TextLayoutOptions const&) override {
    return Size{std::max(1.f, 8.f * static_cast<float>(text.utf8.size())), 14.f};
  }

  Size measure(std::string_view, Font const&, Color const&, float,
               TextLayoutOptions const&) override {
    return Size{48.f, 14.f};
  }

  std::uint32_t resolveFontId(std::string_view, float, bool) override { return 0; }

  std::vector<std::uint8_t> rasterizeGlyph(std::uint32_t, std::uint16_t, float, std::uint32_t&,
                                           std::uint32_t&, Point&) override {
    return {};
  }
};

} // namespace

namespace flux {

struct LayoutContextTestAccess {
  static LayoutContext* create(TextSystem& ts, LayoutEngine& le, LayoutTree& tree,
                               MeasureCache* mc, LayoutContext::SubtreeRootMap* retainedRoots = nullptr,
                               std::uint64_t subtreeRootEpoch = 0) {
    return new LayoutContext(ts, le, tree, mc, retainedRoots, subtreeRootEpoch);
  }

  static void destroy(LayoutContext* ctx) { delete ctx; }
};

} // namespace flux

namespace {

struct LayoutContextDeleter {
  void operator()(flux::LayoutContext* p) const { flux::LayoutContextTestAccess::destroy(p); }
};

using LayoutContextPtr = std::unique_ptr<flux::LayoutContext, LayoutContextDeleter>;

struct StoreScope {
  StateStore store;
  StateStore* previous = nullptr;

  StoreScope() {
    previous = StateStore::current();
    StateStore::setCurrent(&store);
  }

  ~StoreScope() { StateStore::setCurrent(previous); }
};

struct RebuildHarness {
  explicit RebuildHarness(StateStore& storeIn) : store(storeIn) {}

  StateStore& store;
  NullTextSystem ts{};
  LayoutEngine le{};
  MeasureCache mc{};
  LayoutContext::SubtreeRootMap roots{};
  std::uint64_t rootEpoch = 0;
  std::vector<std::shared_ptr<detail::ElementPinStorage>> pinGenerations{};
  LayoutTree tree{};
  SceneGraph graph{};
  EventMap eventMap{};

  void rebuild(Element const& root, bool forceFull = false, float w = 800.f, float h = 1200.f) {
    store.beginRebuild(forceFull);
    mc.beginBuild(store.shouldForceFullRebuild());
    bool const useRetainedLayoutBuild = !roots.empty() && !store.shouldForceFullRebuild();
    ++rootEpoch;
    if (useRetainedLayoutBuild) {
      tree.beginBuild();
    } else {
      tree.clear();
      roots.clear();
    }
    le.resetForBuild();

    LayoutContextPtr ctx{
        flux::LayoutContextTestAccess::create(ts, le, tree, &mc, &roots, rootEpoch)};
    LayoutConstraints rootCs{};
    rootCs.minWidth = w;
    rootCs.minHeight = h;
    rootCs.maxWidth = w;
    rootCs.maxHeight = h;
    ctx->pushConstraints(rootCs);
    le.setChildFrame(Rect{0.f, 0.f, w, h});

    root.layout(*ctx);
    ctx->popConstraints();
    if (useRetainedLayoutBuild) {
      tree.endBuild();
    }

    bool const incrementalSceneReuse = useRetainedLayoutBuild;
    if (!incrementalSceneReuse) {
      graph.clear();
      eventMap.clear();
    } else {
      for (NodeId retired : tree.takeRetiredSceneNodes()) {
        graph.remove(retired);
        eventMap.remove(retired);
      }
    }

    RenderContext rctx{graph, eventMap, ts, incrementalSceneReuse};
    rctx.pushConstraints(rootCs, {});
    renderLayoutTree(tree, rctx);
    rctx.popConstraints();
    if (incrementalSceneReuse) {
      eventMap.prune(graph);
    }

    for (auto it = roots.begin(); it != roots.end();) {
      if (it->second.lastVisitedEpoch != rootEpoch || !tree.get(it->second.rootId)) {
        it = roots.erase(it);
      } else {
        ++it;
      }
    }
    if (store.shouldForceFullRebuild()) {
      pinGenerations.clear();
    }
    pinGenerations.push_back(ctx->pinnedElements());
    while (pinGenerations.size() > 2) {
      pinGenerations.erase(pinGenerations.begin());
    }
    store.endRebuild();
  }
};

struct DummyImage final : Image {
  Size size() const override { return Size{16.f, 16.f}; }
};

struct CounterLeaf {
  std::shared_ptr<int> count;
  int token = 0;

  Element body() const {
    ++*count;
    return Element{Rectangle{}}.size(40.f + static_cast<float>(token), 12.f);
  }
};

struct ComparableCounterLeaf {
  int* count = nullptr;
  int token = 0;

  Element body() const {
    ++*count;
    return Element{Rectangle{}}.size(40.f + static_cast<float>(token), 12.f);
  }
};

struct FixedLeaf {
  Element body() const { return Element{Rectangle{}}.size(40.f, 12.f); }
};

struct SignalLeaf {
  std::shared_ptr<int> count;
  std::shared_ptr<State<int>> handle;

  Element body() const {
    ++*count;
    State<int> state = useState(0);
    *handle = state;
    return Element{Rectangle{}}.size(40.f, 12.f + static_cast<float>(*state));
  }
};

struct ExternalSignalLeaf {
  std::shared_ptr<int> count;
  std::shared_ptr<Signal<int>> signal;

  Element body() const {
    ++*count;
    return Element{Rectangle{}}.size(40.f, 12.f + static_cast<float>(signal->get()));
  }
};

struct ComputedLeaf {
  std::shared_ptr<int> count;
  std::shared_ptr<Computed<int>> computed;

  Element body() const {
    ++*count;
    return Element{Rectangle{}}.size(40.f, 12.f + static_cast<float>(computed->get()));
  }
};

struct PairRoot {
  std::shared_ptr<int> cleanCount;
  std::shared_ptr<int> dirtyCount;
  std::shared_ptr<State<int>> dirtyHandle;

  Element body() const {
    std::vector<Element> children;
    children.push_back(Element{CounterLeaf{.count = cleanCount, .token = 1}});
    children.push_back(Element{SignalLeaf{.count = dirtyCount, .handle = dirtyHandle}});
    return Element{VStack{.spacing = 0.f, .children = std::move(children)}};
  }
};

struct ParentWithSignal {
  std::shared_ptr<int> parentCount;
  int* childCount = nullptr;
  std::shared_ptr<State<int>> parentHandle;

  Element body() const {
    ++*parentCount;
    State<int> state = useState(0);
    *parentHandle = state;
    (void)*state;
    return Element{ComparableCounterLeaf{.count = childCount, .token = 7}};
  }
};

struct ShiftingPairRoot {
  std::shared_ptr<State<int>> firstHandle;

  Element body() const {
    std::vector<Element> children;
    children.push_back(Element{SignalLeaf{
        .count = std::make_shared<int>(0),
        .handle = firstHandle,
    }});
    children.push_back(Element{FixedLeaf{}});
    return Element{VStack{.spacing = 0.f, .children = std::move(children)}};
  }
};

struct DestructionProbe {
  std::shared_ptr<int> destroyCount;
  explicit DestructionProbe(std::shared_ptr<int> destroyCountIn) : destroyCount(std::move(destroyCountIn)) {}
  ~DestructionProbe() { ++*destroyCount; }
};

struct ExternalSignalTrackedRow {
  int id = 0;
  std::shared_ptr<std::vector<std::shared_ptr<Signal<int>>>> signals;
  std::shared_ptr<int> destroyCount;

  Element body() const {
    StateStore::current()->claimSlot<DestructionProbe>(destroyCount);
    return Element{Rectangle{}}.size(
        40.f, 12.f + static_cast<float>((*signals)[static_cast<std::size_t>(id)]->get()));
  }
};

struct MeasuredRenderLeaf {
  std::shared_ptr<int> measureCount;

  Size measure(LayoutConstraints const&, LayoutHints const&) const {
    ++*measureCount;
    return Size{40.f, 12.f};
  }

  void render(Canvas&, Rect const&) const {}

  std::uint64_t measureCacheKey() const noexcept { return 0x57a2d618cf4309beull; }
};

struct MemoizedMeasurePairRoot {
  std::shared_ptr<int> measureCount;
  std::shared_ptr<int> dirtyCount;
  std::shared_ptr<State<int>> dirtyHandle;

  Element body() const {
    std::vector<Element> children;
    children.push_back(Element{MeasuredRenderLeaf{.measureCount = measureCount}});
    children.push_back(Element{SignalLeaf{.count = dirtyCount, .handle = dirtyHandle}});
    return Element{VStack{.spacing = 0.f, .children = std::move(children)}};
  }
};

struct DirtyParentMemoizedMeasureRoot {
  std::shared_ptr<int> measureCount;
  std::shared_ptr<State<int>> parentHandle;

  Element body() const {
    State<int> state = useState(0);
    *parentHandle = state;
    (void)*state;
    std::vector<Element> children;
    children.push_back(Element{MeasuredRenderLeaf{.measureCount = measureCount}});
    children.push_back(Element{Rectangle{}}.size(20.f, 10.f));
    return Element{VStack{.spacing = 0.f, .children = std::move(children)}};
  }
};

struct ShrinkingListRoot {
  std::shared_ptr<std::vector<int>> items;
  std::shared_ptr<std::vector<std::shared_ptr<Signal<int>>>> signals;
  std::shared_ptr<int> destroyCount;

  Element body() const {
    return Element{ForEach<int>{
        *items,
        [signals = signals, destroyCount = destroyCount](int id) {
          return Element{ExternalSignalTrackedRow{
              .id = id,
              .signals = signals,
              .destroyCount = destroyCount,
          }};
        },
        0.f}};
  }
};

struct ForEachSignalRow {
  int id = 0;
  std::shared_ptr<std::vector<std::shared_ptr<int>>> counts;
  std::shared_ptr<std::vector<State<int>>> handles;

  Element body() const {
    ++*(*counts)[static_cast<std::size_t>(id)];
    State<int> state = useState(0);
    (*handles)[static_cast<std::size_t>(id)] = state;
    return Element{Rectangle{}}.size(40.f, 12.f + static_cast<float>(*state));
  }
};

struct ForEachSignalRoot {
  int count = 0;
  std::shared_ptr<std::vector<std::shared_ptr<int>>> counts;
  std::shared_ptr<std::vector<State<int>>> handles;

  Element body() const {
    std::vector<int> items(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
      items[static_cast<std::size_t>(i)] = i;
    }
    return Element{ForEach<int>{
        std::move(items),
        [counts = counts, handles = handles](int id) {
          return Element{ForEachSignalRow{
              .id = id,
              .counts = counts,
              .handles = handles,
          }};
        },
        0.f}};
  }
};

template<typename T>
struct ParentSignalPrimitiveRoot {
  T primitive;
  std::shared_ptr<State<int>> parentHandle;

  Element body() const {
    State<int> state = useState(0);
    *parentHandle = state;
    (void)*state;
    return Element{primitive};
  }
};

template<typename T>
struct PrimitiveLeaf {
  T primitive;

  Element body() const { return Element{primitive}; }
};

template<typename T>
struct PrimitivePairRoot {
  T primitive;
  std::shared_ptr<int> dirtyCount;
  std::shared_ptr<State<int>> dirtyHandle;

  Element body() const {
    std::vector<Element> children;
    children.push_back(Element{PrimitiveLeaf<T>{.primitive = primitive}});
    children.push_back(Element{SignalLeaf{.count = dirtyCount, .handle = dirtyHandle}});
    return Element{VStack{.spacing = 0.f, .children = std::move(children)}};
  }
};

struct NestedRowLeaf {
  int index = 0;
  std::shared_ptr<State<int>> handle;

  Element body() const {
    State<int> state = useState(0);
    if (index == 0) {
      *handle = state;
    }
    float const h = 12.f + static_cast<float>(*state);
    return Element{HStack{
        .spacing = 0.f,
        .children = std::vector<Element>{
            Element{Text{.text = "left"}}.size(40.f, h),
            Element{Text{.text = "right"}}.size(40.f, h),
        },
    }};
  }
};

struct NestedShiftRoot {
  std::shared_ptr<State<int>> firstHandle;

  Element body() const {
    std::vector<Element> children;
    children.push_back(Element{NestedRowLeaf{.index = 0, .handle = firstHandle}});
    children.push_back(Element{NestedRowLeaf{.index = 1, .handle = std::make_shared<State<int>>()}}); 
    return Element{VStack{.spacing = 0.f, .children = std::move(children)}};
  }
};

} // namespace

TEST_CASE("incremental rebuild skips clean composite bodies on unrelated signal writes") {
  auto cleanCount = std::make_shared<int>(0);
  auto dirtyCount = std::make_shared<int>(0);
  auto dirtyHandle = std::make_shared<State<int>>();

  StoreScope scope;
  RebuildHarness harness{scope.store};
  Element root = Element{PairRoot{
      .cleanCount = cleanCount,
      .dirtyCount = dirtyCount,
      .dirtyHandle = dirtyHandle,
  }};

  harness.rebuild(root);
  REQUIRE(*cleanCount == 1);
  REQUIRE(*dirtyCount == 1);

  *dirtyHandle = 1;
  CHECK(scope.store.hasPendingDirtyComponents());
  harness.rebuild(root);

  CHECK(*cleanCount == 1);
  CHECK(*dirtyCount == 2);
}

TEST_CASE("incremental rebuild reruns the composite that owns the changed signal") {
  auto count = std::make_shared<int>(0);
  auto handle = std::make_shared<State<int>>();

  StoreScope scope;
  RebuildHarness harness{scope.store};
  Element root = Element{SignalLeaf{
      .count = count,
      .handle = handle,
  }};

  harness.rebuild(root);
  REQUIRE(*count == 1);

  *handle = 1;
  CHECK(scope.store.hasPendingDirtyComponents());
  harness.rebuild(root);

  CHECK(*count == 2);
}

TEST_CASE("incremental rebuild reruns a composite that depends on a Computed") {
  auto count = std::make_shared<int>(0);
  auto signal = std::make_shared<Signal<int>>(0);
  auto computed = std::make_shared<Computed<int>>([signal] { return signal->get(); });

  StoreScope scope;
  RebuildHarness harness{scope.store};
  Element root = Element{ComputedLeaf{
      .count = count,
      .computed = computed,
  }};

  harness.rebuild(root);
  REQUIRE(*count == 1);

  signal->set(1);
  CHECK(scope.store.hasPendingDirtyComponents());
  harness.rebuild(root);

  CHECK(*count == 2);
}

TEST_CASE("dirty parent can reuse a clean child when the child props are unchanged") {
  auto parentCount = std::make_shared<int>(0);
  auto childCount = std::make_shared<int>(0);
  auto parentHandle = std::make_shared<State<int>>();

  StoreScope scope;
  RebuildHarness harness{scope.store};
  Element root = Element{ParentWithSignal{
      .parentCount = parentCount,
      .childCount = childCount.get(),
      .parentHandle = parentHandle,
  }};

  harness.rebuild(root);
  REQUIRE(*parentCount == 1);
  REQUIRE(*childCount == 1);

  *parentHandle = 1;
  CHECK(scope.store.hasPendingDirtyComponents());
  harness.rebuild(root);

  CHECK(*parentCount == 2);
  CHECK(*childCount == 1);
}

TEST_CASE("incremental rebuild preserves memoized leaf measures for clean siblings") {
  auto measureCount = std::make_shared<int>(0);
  auto dirtyCount = std::make_shared<int>(0);
  auto dirtyHandle = std::make_shared<State<int>>();

  StoreScope scope;
  RebuildHarness harness{scope.store};
  Element root = Element{MemoizedMeasurePairRoot{
      .measureCount = measureCount,
      .dirtyCount = dirtyCount,
      .dirtyHandle = dirtyHandle,
  }};

  harness.rebuild(root);
  REQUIRE(*measureCount == 1);
  REQUIRE(*dirtyCount == 1);

  *dirtyHandle = 1;
  CHECK(scope.store.hasPendingDirtyComponents());
  harness.rebuild(root);

  CHECK(*measureCount == 1);
  CHECK(*dirtyCount == 2);
}

TEST_CASE("incremental rebuild preserves memoized leaf measures across parent body reruns") {
  auto measureCount = std::make_shared<int>(0);
  auto parentHandle = std::make_shared<State<int>>();

  StoreScope scope;
  RebuildHarness harness{scope.store};
  Element root = Element{DirtyParentMemoizedMeasureRoot{
      .measureCount = measureCount,
      .parentHandle = parentHandle,
  }};

  harness.rebuild(root);
  REQUIRE(*measureCount == 1);

  *parentHandle = 1;
  CHECK(scope.store.hasPendingDirtyComponents());
  harness.rebuild(root);

  CHECK(*measureCount == 1);
}

TEST_CASE("layout containers publish stable component keys in the layout tree") {
  StoreScope scope;
  RebuildHarness harness{scope.store};
  Element root = Element{VStack{
      .spacing = 0.f,
      .children = std::vector<Element>{
          Element{Rectangle{}}.size(40.f, 12.f),
          Element{Rectangle{}}.size(40.f, 18.f),
      },
  }};

  harness.rebuild(root, true);

  std::optional<Rect> const rootRect = harness.tree.rectForKey(ComponentKey{0});
  REQUIRE(rootRect.has_value());
  CHECK(rootRect->width == doctest::Approx(800.f));
  CHECK(rootRect->height == doctest::Approx(1200.f));
}

TEST_CASE("incremental rebuild retains clean child composite subtree ids") {
  auto cleanCount = std::make_shared<int>(0);
  auto dirtyCount = std::make_shared<int>(0);
  auto dirtyHandle = std::make_shared<State<int>>();

  StoreScope scope;
  RebuildHarness harness{scope.store};
  Element root = Element{PairRoot{
      .cleanCount = cleanCount,
      .dirtyCount = dirtyCount,
      .dirtyHandle = dirtyHandle,
  }};

  harness.rebuild(root);
  auto const firstIt = harness.roots.find(ComponentKey{0, 0, 0});
  REQUIRE(firstIt != harness.roots.end());
  LayoutNodeId const firstCleanRoot = firstIt->second.rootId;

  *dirtyHandle = 1;
  CHECK(scope.store.hasPendingDirtyComponents());
  harness.rebuild(root);

  auto const secondIt = harness.roots.find(ComponentKey{0, 0, 0});
  REQUIRE(secondIt != harness.roots.end());
  CHECK(secondIt->second.rootId == firstCleanRoot);
}

TEST_CASE("incremental rebuild retains clean child scene node ids") {
  auto cleanCount = std::make_shared<int>(0);
  auto dirtyCount = std::make_shared<int>(0);
  auto dirtyHandle = std::make_shared<State<int>>();

  StoreScope scope;
  RebuildHarness harness{scope.store};
  Element root = Element{PairRoot{
      .cleanCount = cleanCount,
      .dirtyCount = dirtyCount,
      .dirtyHandle = dirtyHandle,
  }};

  harness.rebuild(root);
  auto const firstIt = harness.roots.find(ComponentKey{0, 0, 0});
  REQUIRE(firstIt != harness.roots.end());
  auto firstSceneNodeInSubtree = [&](LayoutNodeId rootId) -> NodeId {
    auto visit = [&](auto&& self, LayoutNodeId id) -> NodeId {
      LayoutNode const* node = harness.tree.get(id);
      if (!node) {
        return kInvalidNodeId;
      }
      if (!node->sceneNodes.empty()) {
        return node->sceneNodes[0];
      }
      for (LayoutNodeId childId : node->children) {
        if (NodeId found = self(self, childId); found.isValid()) {
          return found;
        }
      }
      return kInvalidNodeId;
    };
    return visit(visit, rootId);
  };
  NodeId const firstSceneNode = firstSceneNodeInSubtree(firstIt->second.rootId);
  REQUIRE(firstSceneNode.isValid());

  *dirtyHandle = 1;
  CHECK(scope.store.hasPendingDirtyComponents());
  harness.rebuild(root);

  auto const secondIt = harness.roots.find(ComponentKey{0, 0, 0});
  REQUIRE(secondIt != harness.roots.end());
  CHECK(firstSceneNodeInSubtree(secondIt->second.rootId) == firstSceneNode);
}

TEST_CASE("retained rebuild keeps node.element dereferenceable across generations") {
  auto cleanCount = std::make_shared<int>(0);
  auto dirtyCount = std::make_shared<int>(0);
  auto dirtyHandle = std::make_shared<State<int>>();

  StoreScope scope;
  RebuildHarness harness{scope.store};
  Element root = Element{PairRoot{
      .cleanCount = cleanCount,
      .dirtyCount = dirtyCount,
      .dirtyHandle = dirtyHandle,
  }};

  harness.rebuild(root);
  *dirtyHandle = 1;
  REQUIRE(scope.store.hasPendingDirtyComponents());
  harness.rebuild(root);

  for (LayoutNodeId id : harness.tree.activeIds()) {
    LayoutNode const* node = harness.tree.get(id);
    REQUIRE(node != nullptr);
    if (node->element) {
      (void)node->element->supportsIncrementalSceneReuse();
    }
  }
}

TEST_CASE("incremental rebuild retains stable text scene node ids across dirty sibling rebuilds") {
  auto dirtyCount = std::make_shared<int>(0);
  auto dirtyHandle = std::make_shared<State<int>>();

  StoreScope scope;
  RebuildHarness harness{scope.store};
  Element root = Element{PrimitivePairRoot<Text>{
      .primitive = Text{.text = "hello"},
      .dirtyCount = dirtyCount,
      .dirtyHandle = dirtyHandle,
  }};

  harness.rebuild(root);
  auto const firstIt = harness.roots.find(ComponentKey{0, 0, 0});
  REQUIRE(firstIt != harness.roots.end());
  auto firstSceneNodeInSubtree = [&](LayoutNodeId rootId) -> NodeId {
    auto visit = [&](auto&& self, LayoutNodeId id) -> NodeId {
      LayoutNode const* node = harness.tree.get(id);
      if (!node) {
        return kInvalidNodeId;
      }
      if (!node->sceneNodes.empty()) {
        return node->sceneNodes[0];
      }
      for (LayoutNodeId childId : node->children) {
        if (NodeId found = self(self, childId); found.isValid()) {
          return found;
        }
      }
      return kInvalidNodeId;
    };
    return visit(visit, rootId);
  };
  NodeId const firstSceneNode = firstSceneNodeInSubtree(firstIt->second.rootId);
  REQUIRE(firstSceneNode.isValid());

  *dirtyHandle = 1;
  REQUIRE(scope.store.hasPendingDirtyComponents());
  harness.rebuild(root);

  auto const secondIt = harness.roots.find(ComponentKey{0, 0, 0});
  REQUIRE(secondIt != harness.roots.end());
  CHECK(firstSceneNodeInSubtree(secondIt->second.rootId) == firstSceneNode);
}

TEST_CASE("incremental rebuild retains stable image scene node ids across dirty sibling rebuilds") {
  auto dirtyCount = std::make_shared<int>(0);
  auto dirtyHandle = std::make_shared<State<int>>();
  auto image = std::make_shared<DummyImage>();

  StoreScope scope;
  RebuildHarness harness{scope.store};
  Element root = Element{PrimitivePairRoot<views::Image>{
      .primitive = views::Image{.source = image},
      .dirtyCount = dirtyCount,
      .dirtyHandle = dirtyHandle,
  }};

  harness.rebuild(root);
  auto const firstIt = harness.roots.find(ComponentKey{0, 0, 0});
  REQUIRE(firstIt != harness.roots.end());
  auto firstSceneNodeInSubtree = [&](LayoutNodeId rootId) -> NodeId {
    auto visit = [&](auto&& self, LayoutNodeId id) -> NodeId {
      LayoutNode const* node = harness.tree.get(id);
      if (!node) {
        return kInvalidNodeId;
      }
      if (!node->sceneNodes.empty()) {
        return node->sceneNodes[0];
      }
      for (LayoutNodeId childId : node->children) {
        if (NodeId found = self(self, childId); found.isValid()) {
          return found;
        }
      }
      return kInvalidNodeId;
    };
    return visit(visit, rootId);
  };
  NodeId const firstSceneNode = firstSceneNodeInSubtree(firstIt->second.rootId);
  REQUIRE(firstSceneNode.isValid());

  *dirtyHandle = 1;
  REQUIRE(scope.store.hasPendingDirtyComponents());
  harness.rebuild(root);

  auto const secondIt = harness.roots.find(ComponentKey{0, 0, 0});
  REQUIRE(secondIt != harness.roots.end());
  CHECK(firstSceneNodeInSubtree(secondIt->second.rootId) == firstSceneNode);
}

TEST_CASE("incremental rebuild retains stable path scene node ids across dirty sibling rebuilds") {
  auto dirtyCount = std::make_shared<int>(0);
  auto dirtyHandle = std::make_shared<State<int>>();
  Path path;
  path.rect(Rect{0.f, 0.f, 20.f, 10.f});

  StoreScope scope;
  RebuildHarness harness{scope.store};
  Element root = Element{PrimitivePairRoot<PathShape>{
      .primitive = PathShape{.path = path},
      .dirtyCount = dirtyCount,
      .dirtyHandle = dirtyHandle,
  }};

  harness.rebuild(root);
  auto const firstIt = harness.roots.find(ComponentKey{0, 0, 0});
  REQUIRE(firstIt != harness.roots.end());
  auto firstSceneNodeInSubtree = [&](LayoutNodeId rootId) -> NodeId {
    auto visit = [&](auto&& self, LayoutNodeId id) -> NodeId {
      LayoutNode const* node = harness.tree.get(id);
      if (!node) {
        return kInvalidNodeId;
      }
      if (!node->sceneNodes.empty()) {
        return node->sceneNodes[0];
      }
      for (LayoutNodeId childId : node->children) {
        if (NodeId found = self(self, childId); found.isValid()) {
          return found;
        }
      }
      return kInvalidNodeId;
    };
    return visit(visit, rootId);
  };
  NodeId const firstSceneNode = firstSceneNodeInSubtree(firstIt->second.rootId);
  REQUIRE(firstSceneNode.isValid());

  *dirtyHandle = 1;
  REQUIRE(scope.store.hasPendingDirtyComponents());
  harness.rebuild(root);

  auto const secondIt = harness.roots.find(ComponentKey{0, 0, 0});
  REQUIRE(secondIt != harness.roots.end());
  CHECK(firstSceneNodeInSubtree(secondIt->second.rootId) == firstSceneNode);
}

TEST_CASE("incremental rebuild translates stable nested container subtrees") {
  auto firstHandle = std::make_shared<State<int>>();

  StoreScope scope;
  RebuildHarness harness{scope.store};
  Element root = Element{NestedShiftRoot{.firstHandle = firstHandle}};

  harness.rebuild(root);
  auto findSecondRowContainer = [&]() -> LayoutNode const* {
    LayoutNode const* best = nullptr;
    for (LayoutNodeId id : harness.tree.activeIds()) {
      LayoutNode const* node = harness.tree.get(id);
      if (!node || node->kind != LayoutNode::Kind::Container || node->frame.y <= 0.f) {
        continue;
      }
      if (!best || node->frame.y > best->frame.y) {
        best = node;
      }
    }
    return best;
  };

  LayoutNode const* secondBefore = findSecondRowContainer();
  REQUIRE(secondBefore != nullptr);
  ComponentKey const secondKey = secondBefore->componentKey;
  REQUIRE_FALSE(secondKey.empty());
  float const beforeY = secondBefore->frame.y;

  *firstHandle = 1;
  REQUIRE(scope.store.hasPendingDirtyComponents());
  harness.rebuild(root);

  LayoutNode const* secondAfter = nullptr;
  for (LayoutNodeId id : harness.tree.activeIds()) {
    LayoutNode const* node = harness.tree.get(id);
    if (node && node->componentKey == secondKey) {
      secondAfter = node;
      break;
    }
  }
  REQUIRE(secondAfter != nullptr);
  CHECK(secondAfter->frame.y == doctest::Approx(beforeY + 0.5f));
}

TEST_CASE("incremental rebuild retains clean ForEach item subtree ids") {
  auto counts = std::make_shared<std::vector<std::shared_ptr<int>>>();
  auto handles = std::make_shared<std::vector<State<int>>>(2);
  counts->push_back(std::make_shared<int>(0));
  counts->push_back(std::make_shared<int>(0));

  StoreScope scope;
  RebuildHarness harness{scope.store};
  Element root = Element{ForEachSignalRoot{
      .count = 2,
      .counts = counts,
      .handles = handles,
  }};

  harness.rebuild(root);
  auto const firstIt = harness.roots.find(ComponentKey{0, 0, 0});
  REQUIRE(firstIt != harness.roots.end());
  LayoutNodeId const firstCleanRoot = firstIt->second.rootId;

  (*handles)[1] = 1;
  CHECK(scope.store.hasPendingDirtyComponents());
  harness.rebuild(root);

  auto const secondIt = harness.roots.find(ComponentKey{0, 0, 0});
  REQUIRE(secondIt != harness.roots.end());
  CHECK(secondIt->second.rootId == firstCleanRoot);
  CHECK(*(*counts)[0] == 1);
  CHECK(*(*counts)[1] == 2);
}

TEST_CASE("incremental rebuild retains clean child composite subtree ids") {
  auto cleanCount = std::make_shared<int>(0);
  auto dirtyCount = std::make_shared<int>(0);
  auto dirtyHandle = std::make_shared<State<int>>();

  StoreScope scope;
  RebuildHarness harness{scope.store};
  Element root = Element{PairRoot{
      .cleanCount = cleanCount,
      .dirtyCount = dirtyCount,
      .dirtyHandle = dirtyHandle,
  }};

  harness.rebuild(root);
  auto const firstIt = harness.roots.find(ComponentKey{0, 0, 0});
  REQUIRE(firstIt != harness.roots.end());
  LayoutNodeId const firstCleanRoot = firstIt->second.rootId;

  *dirtyHandle = 1;
  CHECK(scope.store.hasPendingDirtyComponents());
  harness.rebuild(root);

  auto const secondIt = harness.roots.find(ComponentKey{0, 0, 0});
  REQUIRE(secondIt != harness.roots.end());
  CHECK(secondIt->second.rootId == firstCleanRoot);
}


TEST_CASE("removing a composite unsubscribes its external signal and destroys its state") {
  auto items = std::make_shared<std::vector<int>>(std::vector<int>{0, 1});
  auto destroyCount = std::make_shared<int>(0);
  auto signals = std::make_shared<std::vector<std::shared_ptr<Signal<int>>>>();
  signals->push_back(std::make_shared<Signal<int>>(0));
  signals->push_back(std::make_shared<Signal<int>>(0));

  StoreScope scope;
  RebuildHarness harness{scope.store};
  Element root = Element{ShrinkingListRoot{
      .items = items,
      .signals = signals,
      .destroyCount = destroyCount,
  }};

  harness.rebuild(root, true);
  REQUIRE(*destroyCount == 0);

  items->resize(1);
  harness.rebuild(root, true);

  CHECK(*destroyCount == 1);

  (*signals)[1]->set(1);
  CHECK_FALSE(scope.store.hasPendingDirtyComponents());
}
