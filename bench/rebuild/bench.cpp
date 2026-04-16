// Micro-benchmarks for the layout + scene-emit pipeline (one full rebuild minus paint).
//
// Build: cmake -B build -DFLUX_BUILD_BENCHMARKS=ON && cmake --build build --target rebuild_bench
//
// Scope: measures what BuildOrchestrator::rebuild does between graph.clear() and paint:
// element tree walk, measure, layout, scene-node emit, event-map fill. Does not touch Metal.
// A NullTextSystem stubs out CoreText so Text views short-circuit to zero-size layouts; Text is
// already covered by bench/paragraph_cache.

#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/Scene/SceneGraph.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/EventMap.hpp>
#include <Flux/UI/LayoutContext.hpp>
#include <Flux/UI/LayoutEngine.hpp>
#include <Flux/UI/LayoutTree.hpp>
#include <Flux/UI/MeasureCache.hpp>
#include <Flux/UI/RenderContext.hpp>
#include <Flux/UI/RenderLayoutTree.hpp>
#include <Flux/UI/StateStore.hpp>
#include <Flux/UI/Views/ForEach.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/VStack.hpp>
#include <Flux/UI/Hooks.hpp>

#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <utility>
#include <vector>

namespace {

using namespace flux;

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

  std::vector<std::uint8_t> rasterizeGlyph(std::uint32_t, std::uint16_t, float, std::uint32_t&,
                                           std::uint32_t&, Point&) override {
    return {};
  }
};

} // namespace

namespace flux {

struct LayoutContextTestAccess {
  static LayoutContext* create(TextSystem& ts, LayoutEngine& le, LayoutTree& tree,
                               MeasureCache* mc, LayoutTree const* retainedTree = nullptr,
                               LayoutContext::SubtreeRootMap const* retainedRoots = nullptr) {
    return new LayoutContext(ts, le, tree, mc, retainedTree, retainedRoots);
  }

  static void destroy(LayoutContext* ctx) { delete ctx; }
};

} // namespace flux

namespace {

struct LayoutContextDeleter {
  void operator()(flux::LayoutContext* p) const { flux::LayoutContextTestAccess::destroy(p); }
};

using LayoutContextPtr = std::unique_ptr<flux::LayoutContext, LayoutContextDeleter>;

struct StateStoreGuard {
  StateStore store;
  StateStore* previous = nullptr;

  StateStoreGuard() {
    previous = StateStore::current();
    StateStore::setCurrent(&store);
  }

  ~StateStoreGuard() { StateStore::setCurrent(previous); }
};

struct FrameContext {
  explicit FrameContext(StateStore& stateStore) : store(stateStore) {}

  StateStore& store;
  NullTextSystem ts{};
  LayoutEngine le{};
  MeasureCache mc{};
  LayoutContext::SubtreeRootMap roots{};
  LayoutContext::SubtreeRootMap retainedRoots{};
  std::shared_ptr<detail::ElementPinStorage> pins{};
  std::shared_ptr<detail::ElementPinStorage> retainedPins{};
  LayoutTree retainedTree{};
  LayoutTree tree{};
  SceneGraph graph{};
  EventMap eventMap{};
  LayoutConstraints lastRootConstraints{};
  std::uint64_t lastRootMeasureId = 0;
  bool hasCurrentLayout = false;

  void rebuild(Element const& root, float w, float h) {
    LayoutConstraints rootCs{};
    rootCs.minWidth = w;
    rootCs.minHeight = h;
    rootCs.maxWidth = w;
    rootCs.maxHeight = h;
    bool const canReuseWholeLayout =
        hasCurrentLayout && !store.hasPendingDirtyComponents() && root.measureId() == lastRootMeasureId &&
        rootCs.minWidth == lastRootConstraints.minWidth &&
        rootCs.minHeight == lastRootConstraints.minHeight &&
        rootCs.maxWidth == lastRootConstraints.maxWidth &&
        rootCs.maxHeight == lastRootConstraints.maxHeight;

    if (canReuseWholeLayout) {
      return;
    }

    graph.clear();
    eventMap = EventMap{};
    le.resetForBuild();
    store.beginRebuild(false);
    if (store.shouldForceFullRebuild()) {
      mc.clear();
    }
    if (hasCurrentLayout) {
      retainedTree.clear();
      std::swap(retainedTree, tree);
      retainedRoots = std::move(roots);
      retainedPins = std::move(pins);
      hasCurrentLayout = false;
    } else {
      tree.clear();
    }

    LayoutContextPtr ctx{
        flux::LayoutContextTestAccess::create(ts, le, tree, &mc, &retainedTree, &retainedRoots)};
    ctx->pushConstraints(rootCs);
    le.setChildFrame(Rect{0.f, 0.f, w, h});

    root.layout(*ctx);
    ctx->popConstraints();

    roots = ctx->subtreeRootLayouts();
    pins = ctx->pinnedElements();
    lastRootConstraints = rootCs;
    lastRootMeasureId = root.measureId();
    hasCurrentLayout = true;
    store.endRebuild();

    RenderContext rctx{graph, eventMap, ts};
    rctx.pushConstraints(rootCs, {});
    renderLayoutTree(tree, rctx);
    rctx.popConstraints();
  }
};

Element flatVStack(int n, float rowH = 20.f) {
  std::vector<Element> rows;
  rows.reserve(static_cast<std::size_t>(n));
  for (int i = 0; i < n; ++i) {
    rows.push_back(Element{Rectangle{}}.size(200.f, rowH));
  }
  return Element{VStack{.spacing = 0.f, .children = std::move(rows)}};
}

Element card(int depth, int width, float leafH = 12.f) {
  if (depth == 0) {
    return Element{Rectangle{}}.size(40.f, leafH);
  }

  std::vector<Element> kids;
  kids.reserve(static_cast<std::size_t>(width));
  for (int i = 0; i < width; ++i) {
    kids.push_back(card(depth - 1, width, leafH));
  }

  if ((depth & 1) != 0) {
    return Element{HStack{.spacing = 0.f, .children = std::move(kids)}};
  }
  return Element{VStack{.spacing = 0.f, .children = std::move(kids)}};
}

Element foreachRows(int n, float rowH = 24.f) {
  std::vector<int> items;
  items.reserve(static_cast<std::size_t>(n));
  for (int i = 0; i < n; ++i) {
    items.push_back(i);
  }
  return Element{ForEach<int>{
      std::move(items),
      [rowH](int const&) { return Element{Rectangle{}}.size(200.f, rowH); },
      0.f}};
}

struct SignalRow {
  int index = 0;
  std::shared_ptr<std::vector<State<int>>> handles;
  float rowH = 16.f;

  Element body() const {
    State<int> state = useState(0);
    (*handles)[static_cast<std::size_t>(index)] = state;
    return Element{Rectangle{}}.size(200.f, rowH + 0.001f * static_cast<float>(*state));
  }
};

struct SignalRowsTree {
  int count = 0;
  std::shared_ptr<std::vector<State<int>>> handles;
  float rowH = 16.f;

  Element body() const {
    std::vector<Element> rows;
    rows.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
      rows.push_back(Element{SignalRow{.index = i, .handles = handles, .rowH = rowH}});
    }
    return Element{VStack{.spacing = 0.f, .children = std::move(rows)}};
  }
};

static double secondsSince(std::chrono::steady_clock::time_point t0) {
  auto const t1 = std::chrono::steady_clock::now();
  return std::chrono::duration<double>(t1 - t0).count();
}

template<typename F>
static double timeIt(F&& f) {
  auto const t0 = std::chrono::steady_clock::now();
  f();
  return secondsSince(t0);
}

template<typename F>
static double timeAveraged(int iters, F&& f) {
  auto const t0 = std::chrono::steady_clock::now();
  for (int i = 0; i < iters; ++i) {
    f();
  }
  return secondsSince(t0) / static_cast<double>(iters);
}

static void printRow(char const* label, double seconds) {
  std::cout << label;
  if (seconds >= 1e-3) {
    std::cout << (seconds * 1e3) << " ms\n";
  } else if (seconds >= 1e-6) {
    std::cout << (seconds * 1e6) << " us\n";
  } else {
    std::cout << (seconds * 1e9) << " ns\n";
  }
}

} // namespace

int main() {
  constexpr float W = 1200.f;
  constexpr float H = 2000.f;

  {
    StateStoreGuard guard;
    FrameContext fc{guard.store};
    Element root = flatVStack(5000, 16.f);
    double const t = timeIt([&] { fc.rebuild(root, W, H); });
    std::cout << "B1 flat 5000-row VStack cold:           ";
    printRow("", t);
    std::cout << "   layout_tree_nodes: " << fc.tree.nodes().size() << "\n";
  }

  {
    StateStoreGuard guard;
    FrameContext fc{guard.store};
    Element root = flatVStack(5000, 16.f);
    constexpr int N = 20;
    double const per = timeAveraged(N, [&] { fc.rebuild(root, W, H); });
    std::cout << "B2 flat 5000-row VStack steady-state:   ";
    printRow("", per);
  }

  {
    StateStoreGuard guard;
    FrameContext fc{guard.store};
    constexpr int N = 20;
    double total = 0.0;
    for (int k = 0; k < N; ++k) {
      Element root = flatVStack(5000, 16.f + 0.001f * static_cast<float>(k));
      total += timeIt([&] { fc.rebuild(root, W, H); });
    }
    std::cout << "B3 flat 5000-row VStack 1-leaf-delta:   ";
    printRow("", total / N);
  }

  {
    StateStoreGuard guard;
    FrameContext fc{guard.store};
    Element root = card(4, 5, 12.f);
    double const cold = timeIt([&] { fc.rebuild(root, W, H); });
    std::cout << "B4 card tree (depth=4, width=5) cold:   ";
    printRow("", cold);
    std::cout << "   layout_tree_nodes: " << fc.tree.nodes().size() << "\n";
    constexpr int N = 50;
    double const steady = timeAveraged(N, [&] { fc.rebuild(root, W, H); });
    std::cout << "B4 card tree steady-state:              ";
    printRow("", steady);
  }

  {
    StateStoreGuard guard;
    FrameContext fc{guard.store};
    Element root = foreachRows(5000, 24.f);
    double const cold = timeIt([&] { fc.rebuild(root, W, H); });
    std::cout << "B5 ForEach 5000 rows cold:              ";
    printRow("", cold);
    constexpr int N = 10;
    double const steady = timeAveraged(N, [&] { fc.rebuild(root, W, H); });
    std::cout << "B5 ForEach 5000 rows steady-state:      ";
    printRow("", steady);
  }

  {
    StateStoreGuard guard;
    FrameContext fc{guard.store};
    Element root = foreachRows(100, 32.f);
    constexpr int N = 500;
    double const per = timeAveraged(N, [&] { fc.rebuild(root, W, H); });
    std::cout << "B6 ForEach 100 rows steady-state:       ";
    printRow("", per);
  }

  {
    StateStoreGuard guard;
    FrameContext fc{guard.store};
    Element root = flatVStack(1000, 16.f);
    fc.rebuild(root, W, H);
    constexpr int N = 50;
    auto const t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < N; ++i) {
      float const w = W + static_cast<float>(i % 4);
      fc.rebuild(root, w, H);
    }
    double const per = secondsSince(t0) / N;
    std::cout << "B7 flat 1000-row resize (width delta):  ";
    printRow("", per);
  }

  {
    std::cout << "B8 scaling (flat VStack, cold rebuild):\n";
    for (int n : {100, 500, 2000, 10000}) {
      StateStoreGuard guard;
      FrameContext fc{guard.store};
      Element root = flatVStack(n, 10.f);
      double const t = timeIt([&] { fc.rebuild(root, W, H * 5.f); });
      std::cout << "   N=" << n << ": " << (t * 1e6) << " us ("
                << (t * 1e9 / n) << " ns/leaf)\n";
    }
  }

  {
    StateStoreGuard guard;
    FrameContext fc{guard.store};
    auto handles = std::make_shared<std::vector<State<int>>>(1000);
    Element root = Element{SignalRowsTree{.count = 1000, .handles = handles, .rowH = 16.f}};
    fc.rebuild(root, W, H);
    constexpr int N = 50;
    double total = 0.0;
    for (int i = 0; i < N; ++i) {
      (*handles)[500] = (i & 1) != 0 ? 0 : 1;
      total += timeIt([&] { fc.rebuild(root, W, H); });
    }
    std::cout << "B9 signal-write 1000 composite tree:    ";
    printRow("", total / N);
  }

  {
    StateStoreGuard guard;
    FrameContext fc{guard.store};
    auto handles = std::make_shared<std::vector<State<int>>>(10000);
    Element root = Element{SignalRowsTree{.count = 10000, .handles = handles, .rowH = 16.f}};
    fc.rebuild(root, W, H * 8.f);
    constexpr int N = 20;
    double total = 0.0;
    for (int i = 0; i < N; ++i) {
      (*handles)[5000] = (i & 1) != 0 ? 0 : 1;
      total += timeIt([&] { fc.rebuild(root, W, H * 8.f); });
    }
    std::cout << "B10 signal-write 10000 composite tree:  ";
    printRow("", total / N);
  }

  return 0;
}
