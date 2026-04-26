#include <doctest/doctest.h>

#include <Flux/Core/Application.hpp>
#include <Flux/Core/ComponentKey.hpp>
#include <Flux/Core/LocalId.hpp>
#include <Flux/Reactive/Signal.hpp>
#include <Flux/UI/Detail/TraversalContext.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/StateStore.hpp>
#include <Flux/UI/Views/Rectangle.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

struct CopyCountingLeaf {
    inline static int copies = 0;

    CopyCountingLeaf() = default;
    CopyCountingLeaf(CopyCountingLeaf const&) { ++copies; }
    CopyCountingLeaf& operator=(CopyCountingLeaf const&) {
        ++copies;
        return *this;
    }

    flux::Size measure(flux::MeasureContext&, flux::LayoutConstraints const&,
                       flux::LayoutHints const&, flux::TextSystem&) const {
        return flux::Size{1.f, 1.f};
    }
};

} // namespace

TEST_CASE("Composite-observed signals schedule the next reactive frame") {
    using namespace std::chrono_literals;

    flux::Application app;
    flux::StateStore store;
    flux::Signal<int> signal {0};
    flux::ComponentKey key {flux::LocalId::fromIndex(0)};

    (void)signal.observeComposite(store, key);

    std::atomic<int> frameCount {0};
    auto handle = app.onNextFrameNeeded([&] {
        ++frameCount;
        app.quit();
    });

    std::jthread failsafe([&] {
        std::this_thread::sleep_for(150ms);
        app.quit();
    });

    signal.set(1);
    int const exitCode = app.exec();
    app.unobserveNextFrame(handle);

    CHECK(exitCode == 0);
    CHECK(frameCount.load() == 1);
}

TEST_CASE("ComponentKey interned handles preserve hash and prefix semantics") {
    using flux::ComponentKey;
    using flux::ComponentKeyHash;
    using flux::LocalId;

    ComponentKey key {LocalId::fromString("panel")};
    key.push_back(LocalId::fromString("button"));

    ComponentKey const expected {LocalId::fromString("panel"), LocalId::fromString("button")};
    ComponentKey const prefix {LocalId::fromString("panel")};

    CHECK(key == expected);
    CHECK(key.hasPrefix(prefix));
    CHECK(prefix.sharesPrefix(key));
    CHECK(key.prefix(1) == prefix);
    CHECK(key.tail() == LocalId::fromString("button"));
    CHECK(key.materialize() == std::vector<LocalId>{LocalId::fromString("panel"),
                                                    LocalId::fromString("button")});

    std::unordered_map<ComponentKey, int, ComponentKeyHash> values;
    values.emplace(key, 42);

    auto const it = values.find(expected);
    REQUIRE(it != values.end());
    CHECK(it->second == 42);
}

TEST_CASE("ComponentKey interned sibling lookups stay stable across wide parent fanout") {
    using flux::ComponentKey;
    using flux::LocalId;

    ComponentKey const parent {LocalId::fromString("stack")};
    std::vector<ComponentKey> positionalChildren;
    std::vector<ComponentKey> keyedChildren;
    positionalChildren.reserve(12);
    keyedChildren.reserve(6);

    for (std::size_t index = 0; index < 12; ++index) {
        positionalChildren.emplace_back(parent, LocalId::fromIndex(index));
    }
    for (char suffix = 'a'; suffix <= 'f'; ++suffix) {
        keyedChildren.emplace_back(parent, LocalId::fromString(std::string(1, suffix)));
    }

    CHECK(positionalChildren.back() == ComponentKey {LocalId::fromString("stack"), LocalId::fromIndex(11)});
    CHECK(keyedChildren.back() == ComponentKey {LocalId::fromString("stack"), LocalId::fromString("f")});
    CHECK(positionalChildren[7].hasPrefix(parent));
    CHECK(keyedChildren[2].hasPrefix(parent));
    CHECK(positionalChildren[0].tail() == LocalId::fromIndex(0));
    CHECK(keyedChildren[0].tail() == LocalId::fromString("a"));
}

TEST_CASE("StateStore dirty descendant queries use strict active ancestor keys") {
    using flux::ComponentKey;
    using flux::LocalId;
    using flux::StateStore;

    StateStore store;
    ComponentKey const root {LocalId::fromString("root")};
    ComponentKey const child {root, LocalId::fromString("child")};
    ComponentKey const grandchild {child, LocalId::fromString("grandchild")};
    ComponentKey const sibling {LocalId::fromString("sibling")};

    store.markCompositeDirty(grandchild);
    store.beginRebuild(false);

    CHECK(store.hasDirtyDescendant(ComponentKey{}));
    CHECK(store.hasDirtyDescendant(root));
    CHECK(store.hasDirtyDescendant(child));
    CHECK(!store.hasDirtyDescendant(grandchild));
    CHECK(!store.hasDirtyDescendant(sibling));
    CHECK(!store.isComponentDirty(child));
    CHECK(store.isComponentDirty(grandchild));

    store.endRebuild();
}

TEST_CASE("Element copies share model storage and detach modifiers on write") {
    using flux::Element;
    using flux::Rectangle;

    CopyCountingLeaf::copies = 0;
    Element first {CopyCountingLeaf{}};
    CopyCountingLeaf::copies = 0;
    Element second = first;

    CHECK(CopyCountingLeaf::copies == 0);
    CHECK(first.structuralEquals(second));

    Element base = Element{Rectangle{}}.padding(1.f);
    Element shared = base;
    Element changed = std::move(shared).padding(2.f);

    REQUIRE(base.modifiers() != nullptr);
    REQUIRE(changed.modifiers() != nullptr);
    CHECK(base.modifiers()->padding.top == doctest::Approx(1.f));
    CHECK(changed.modifiers()->padding.top == doctest::Approx(2.f));
}

TEST_CASE("TraversalContext reuses its interned prefix key for current child keys") {
    using flux::ComponentKey;
    using flux::LocalId;
    using flux::detail::TraversalContext;

    TraversalContext traversal;
    traversal.pushChildIndexWithLocalId(LocalId::fromString("stack"));

    CHECK(traversal.currentElementLocalId() == LocalId::fromIndex(0));
    CHECK(traversal.currentElementKey() ==
          ComponentKey{LocalId::fromString("stack"), LocalId::fromIndex(0)});

    ComponentKey const first = traversal.nextCompositeKey();
    CHECK(first == ComponentKey{LocalId::fromString("stack"), LocalId::fromIndex(0)});
    CHECK(traversal.currentElementKey() ==
          ComponentKey{LocalId::fromString("stack"), LocalId::fromIndex(1)});

    traversal.pushExplicitChildLocalId(LocalId::fromString("leaf"));
    CHECK(traversal.currentElementLocalId() == LocalId::fromString("leaf"));
    CHECK(traversal.currentElementKey() ==
          ComponentKey{LocalId::fromString("stack"), LocalId::fromString("leaf")});
    traversal.popExplicitChildLocalId();

    traversal.popChildIndex();
    CHECK(traversal.currentElementLocalId() == LocalId::fromIndex(0));
}
