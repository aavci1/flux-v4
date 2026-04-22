#include <doctest/doctest.h>

#include <Flux/Core/Application.hpp>
#include <Flux/Reactive/Signal.hpp>
#include <Flux/Scene/LocalId.hpp>
#include <Flux/UI/ComponentKey.hpp>
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
