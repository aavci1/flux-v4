#include <doctest/doctest.h>

#include <Flux/Core/Application.hpp>
#include <Flux/Core/ComponentKey.hpp>
#include <Flux/Core/LocalId.hpp>
#include <Flux/Reactive/Signal.hpp>
#include <Flux/UI/StateStore.hpp>

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
