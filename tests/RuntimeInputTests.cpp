#include <doctest/doctest.h>

#include <Flux/Core/Application.hpp>
#include <Flux/Core/Events.hpp>
#include <Flux/Core/KeyCodes.hpp>
#include <Flux/Core/Shortcut.hpp>
#include <Flux/Core/Window.hpp>
#include <Flux/Detail/RootHolder.hpp>
#include <Flux/Detail/Runtime.hpp>
#include <Flux/Reactive/Signal.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/ScrollView.hpp>
#include <Flux/UI/Views/Show.hpp>

#include <cstdint>
#include <memory>
#include <utility>

namespace {

struct RuntimeHarness {
  flux::Application app;
  flux::Window& window;
  flux::Runtime runtime;

  RuntimeHarness()
      : app()
      , window(app.createWindow(flux::WindowConfig{
            .size = {240.f, 140.f},
            .title = "Flux Runtime Input Tests",
            .fullscreen = false,
            .resizable = false,
        }))
      , runtime(window) {}

  ~RuntimeHarness() {
    runtime.beginShutdown();
  }

  template<typename Root>
  void setRoot(Root root) {
    runtime.setRoot(std::make_unique<flux::TypedRootHolder<Root>>(
        std::in_place, std::move(root)));
  }

  void pointerMove(flux::Point point) {
    dispatchPointer(flux::InputEvent::Kind::PointerMove, point);
  }

  void pointerDown(flux::Point point) {
    dispatchPointer(flux::InputEvent::Kind::PointerDown, point);
  }

  void pointerUp(flux::Point point) {
    dispatchPointer(flux::InputEvent::Kind::PointerUp, point);
  }

  void keyDown(flux::KeyCode key, flux::Modifiers modifiers = flux::Modifiers::None) {
    flux::InputEvent event{};
    event.kind = flux::InputEvent::Kind::KeyDown;
    event.handle = window.handle();
    event.key = key;
    event.modifiers = modifiers;
    runtime.handleInput(event);
  }

  void scroll(flux::Point point, flux::Vec2 delta, bool precise = true) {
    flux::InputEvent event{};
    event.kind = flux::InputEvent::Kind::Scroll;
    event.handle = window.handle();
    event.position = {point.x, point.y};
    event.scrollDelta = delta;
    event.preciseScrollDelta = precise;
    runtime.handleInput(event);
  }

private:
  void dispatchPointer(flux::InputEvent::Kind kind, flux::Point point) {
    flux::InputEvent event{};
    event.kind = kind;
    event.handle = window.handle();
    event.position = {point.x, point.y};
    event.button = flux::MouseButton::Left;
    event.pressedButtons =
        kind == flux::InputEvent::Kind::PointerUp ? 0u : static_cast<std::uint8_t>(1u);
    runtime.handleInput(event);
  }
};

struct ProbeView {
  flux::Reactive::Signal<bool>* hover = nullptr;
  flux::Reactive::Signal<bool>* press = nullptr;
  flux::Reactive::Signal<bool>* focus = nullptr;
  flux::Reactive::Signal<bool>* keyboardFocus = nullptr;

  flux::Element body() const {
    if (hover) {
      *hover = flux::useHover();
    }
    if (press) {
      *press = flux::usePress();
    }
    if (focus) {
      *focus = flux::useFocus();
    }
    if (keyboardFocus) {
      *keyboardFocus = flux::useKeyboardFocus();
    }
    return flux::Element{flux::Rectangle{}}
        .size(20.f, 20.f)
        .focusable(true)
        .onTap([] {});
  }
};

struct SingleProbeRoot {
  flux::Reactive::Signal<bool>* hover = nullptr;
  flux::Reactive::Signal<bool>* press = nullptr;
  flux::Reactive::Signal<bool>* focus = nullptr;
  flux::Reactive::Signal<bool>* keyboardFocus = nullptr;

  flux::Element body() const {
    return ProbeView{hover, press, focus, keyboardFocus};
  }
};

struct TwoProbeRoot {
  flux::Reactive::Signal<bool>* firstFocus = nullptr;
  flux::Reactive::Signal<bool>* secondFocus = nullptr;

  flux::Element body() const {
    return flux::HStack{
        .spacing = 8.f,
        .children = flux::children(
            ProbeView{nullptr, nullptr, firstFocus, nullptr},
            ProbeView{nullptr, nullptr, secondFocus, nullptr}),
    };
  }
};

struct ActionProbeView {
  int* fired = nullptr;

  flux::Element body() const {
    flux::useFocus();
    flux::useViewAction("demo.save", [fired = fired] {
      ++*fired;
    });
    return flux::Element{flux::Rectangle{}}
        .size(20.f, 20.f)
        .focusable(true)
        .onTap([] {});
  }
};

struct TwoActionRoot {
  int* firstFired = nullptr;
  int* secondFired = nullptr;

  flux::Element body() const {
    return flux::HStack{
        .spacing = 8.f,
        .children = flux::children(
            ActionProbeView{firstFired},
            ActionProbeView{secondFired}),
    };
  }
};

struct ConditionalActionRoot {
  flux::Reactive::Signal<bool> visible;
  int* fired = nullptr;

  flux::Element body() const {
    return flux::Show(visible, [fired = fired] {
      return flux::Element{ActionProbeView{fired}};
    });
  }
};

struct ConditionalHoverRoot {
  flux::Reactive::Signal<bool> visible;
  flux::Reactive::Signal<bool>* hover = nullptr;

  flux::Element body() const {
    return flux::Show(visible, [hover = hover] {
      return flux::Element{ProbeView{hover, nullptr, nullptr, nullptr}};
    });
  }
};

struct ScrollProbeRoot {
  flux::Reactive::Signal<flux::Point> offset;

  flux::Element body() const {
    return flux::ScrollView{
        .axis = flux::ScrollAxis::Vertical,
        .scrollOffset = offset,
        .children = flux::children(
            flux::Rectangle{}.size(100.f, 100.f),
            flux::Rectangle{}.size(100.f, 100.f)),
    };
  }
};

void checkSameColor(flux::Color actual, flux::Color expected) {
  CHECK(actual.r == doctest::Approx(expected.r));
  CHECK(actual.g == doctest::Approx(expected.g));
  CHECK(actual.b == doctest::Approx(expected.b));
  CHECK(actual.a == doctest::Approx(expected.a));
}

void registerSaveAction(flux::Window& window) {
  window.registerAction("demo.save", flux::ActionDescriptor{
      .label = "Save",
      .shortcut = flux::shortcuts::Save,
  });
}

} // namespace

TEST_CASE("pointer move into element flips hover signal") {
  RuntimeHarness harness;
  flux::Reactive::Signal<bool> hover;
  harness.setRoot(SingleProbeRoot{.hover = &hover});

  harness.pointerMove({10.f, 10.f});
  CHECK(hover.get());

  harness.pointerMove({100.f, 100.f});
  CHECK_FALSE(hover.get());
}

TEST_CASE("pointer down and up flip press signal") {
  RuntimeHarness harness;
  flux::Reactive::Signal<bool> press;
  harness.setRoot(SingleProbeRoot{.press = &press});

  harness.pointerDown({10.f, 10.f});
  CHECK(press.get());

  harness.pointerUp({10.f, 10.f});
  CHECK_FALSE(press.get());
}

TEST_CASE("pointer down then drag out keeps press until release") {
  RuntimeHarness harness;
  flux::Reactive::Signal<bool> press;
  harness.setRoot(SingleProbeRoot{.press = &press});

  harness.pointerDown({10.f, 10.f});
  REQUIRE(press.get());

  harness.pointerMove({100.f, 100.f});
  CHECK(press.get());

  harness.pointerUp({100.f, 100.f});
  CHECK_FALSE(press.get());
}

TEST_CASE("focus moves between elements") {
  RuntimeHarness harness;
  flux::Reactive::Signal<bool> firstFocus;
  flux::Reactive::Signal<bool> secondFocus;
  harness.setRoot(TwoProbeRoot{.firstFocus = &firstFocus, .secondFocus = &secondFocus});

  harness.keyDown(flux::keys::Tab);
  CHECK(firstFocus.get());
  CHECK_FALSE(secondFocus.get());

  harness.keyDown(flux::keys::Tab);
  CHECK_FALSE(firstFocus.get());
  CHECK(secondFocus.get());
}

TEST_CASE("keyboard focus signal differs from pointer focus") {
  RuntimeHarness harness;
  flux::Reactive::Signal<bool> focus;
  flux::Reactive::Signal<bool> keyboardFocus;
  harness.setRoot(SingleProbeRoot{.focus = &focus, .keyboardFocus = &keyboardFocus});

  harness.pointerDown({10.f, 10.f});
  CHECK(focus.get());
  CHECK_FALSE(keyboardFocus.get());

  harness.pointerDown({100.f, 100.f});
  REQUIRE_FALSE(focus.get());

  harness.keyDown(flux::keys::Tab);
  CHECK(focus.get());
  CHECK(keyboardFocus.get());
}

TEST_CASE("view action fires only for the focused view") {
  RuntimeHarness harness;
  registerSaveAction(harness.window);
  int firstFired = 0;
  int secondFired = 0;
  harness.setRoot(TwoActionRoot{.firstFired = &firstFired, .secondFired = &secondFired});

  harness.keyDown(flux::keys::Tab);
  harness.keyDown(flux::keys::S, flux::Modifiers::Meta);
  CHECK(firstFired == 1);
  CHECK(secondFired == 0);

  harness.keyDown(flux::keys::Tab);
  harness.keyDown(flux::keys::S, flux::Modifiers::Meta);
  CHECK(firstFired == 1);
  CHECK(secondFired == 1);
}

TEST_CASE("view action deregisters on view unmount") {
  RuntimeHarness harness;
  registerSaveAction(harness.window);
  flux::Reactive::Signal<bool> visible{true};
  int fired = 0;
  harness.setRoot(ConditionalActionRoot{.visible = visible, .fired = &fired});

  harness.pointerDown({10.f, 10.f});
  harness.keyDown(flux::keys::S, flux::Modifiers::Meta);
  REQUIRE(fired == 1);

  visible.set(false);
  harness.keyDown(flux::keys::S, flux::Modifiers::Meta);
  CHECK(fired == 1);
}

TEST_CASE("hover chain disposes signals on subtree unmount") {
  RuntimeHarness harness;
  flux::Reactive::Signal<bool> visible{true};
  flux::Reactive::Signal<bool> hover;
  harness.setRoot(ConditionalHoverRoot{.visible = visible, .hover = &hover});

  harness.pointerMove({10.f, 10.f});
  REQUIRE(hover.get());

  visible.set(false);
  CHECK(hover.disposed());

  harness.pointerMove({100.f, 100.f});
  CHECK(hover.disposed());
}

TEST_CASE("runtime scroll dispatch reaches scroll view") {
  RuntimeHarness harness;
  flux::Reactive::Signal<flux::Point> offset{flux::Point{0.f, 0.f}};
  harness.setRoot(ScrollProbeRoot{.offset = offset});

  harness.scroll({10.f, 10.f}, {0.f, -12.f});

  CHECK(offset.get().x == doctest::Approx(0.f));
  CHECK(offset.get().y == doctest::Approx(12.f));
}

TEST_CASE("window clear color follows theme unless overridden") {
  RuntimeHarness harness;
  checkSameColor(harness.window.clearColor(), flux::Theme::light().windowBackgroundColor);

  harness.window.setTheme(flux::Theme::dark());
  checkSameColor(harness.window.clearColor(), flux::Theme::dark().windowBackgroundColor);

  flux::Color const custom = flux::Color::hex(0x123456);
  harness.window.setClearColor(custom);
  harness.window.setTheme(flux::Theme::light());
  checkSameColor(harness.window.clearColor(), custom);
}
