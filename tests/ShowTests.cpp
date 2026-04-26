#include <doctest/doctest.h>

#include <Flux/Detail/RootHolder.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/Reactive/Scope.hpp>
#include <Flux/Reactive/Signal.hpp>
#include <Flux/SceneGraph/GroupNode.hpp>
#include <Flux/SceneGraph/SceneGraph.hpp>
#include <Flux/UI/MountRoot.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/Show.hpp>
#include <Flux/UI/Views/Switch.hpp>
#include <Flux/UI/Views/VStack.hpp>

#include <memory>
#include <string_view>
#include <vector>

namespace {

class FakeTextSystem final : public flux::TextSystem {
public:
  std::shared_ptr<flux::TextLayout const>
  layout(flux::AttributedString const&, float, flux::TextLayoutOptions const&) override {
    return std::make_shared<flux::TextLayout>();
  }

  std::shared_ptr<flux::TextLayout const>
  layout(std::string_view, flux::Font const&, flux::Color const&, float,
         flux::TextLayoutOptions const&) override {
    return std::make_shared<flux::TextLayout>();
  }

  flux::Size measure(flux::AttributedString const&, float,
                     flux::TextLayoutOptions const&) override {
    return {0.f, 0.f};
  }

  flux::Size measure(std::string_view, flux::Font const&, flux::Color const&, float,
                     flux::TextLayoutOptions const&) override {
    return {0.f, 0.f};
  }

  std::uint32_t resolveFontId(std::string_view, float, bool) override { return 0; }

  std::vector<std::uint8_t> rasterizeGlyph(std::uint32_t, std::uint16_t, float,
                                           std::uint32_t& outWidth,
                                           std::uint32_t& outHeight,
                                           flux::Point& outBearing) override {
    outWidth = 0;
    outHeight = 0;
    outBearing = {};
    return {};
  }
};

flux::EnvironmentLayer testEnvironment() {
  flux::EnvironmentLayer environment;
  environment.set(flux::Theme::light());
  return environment;
}

flux::scenegraph::GroupNode const& rootGroup(flux::scenegraph::SceneGraph const& sceneGraph) {
  REQUIRE(sceneGraph.root().kind() == flux::scenegraph::SceneNodeKind::Group);
  return static_cast<flux::scenegraph::GroupNode const&>(sceneGraph.root());
}

} // namespace

TEST_CASE("Show replaces branches and disposes the inactive scope") {
  struct Root {
    flux::Reactive::Signal<bool> visible;
    int* thenCreated = nullptr;
    int* thenDisposed = nullptr;
    int* elseCreated = nullptr;
    int* elseDisposed = nullptr;

    flux::Element body() const {
      return flux::Show(
          visible,
          [thenCreated = thenCreated, thenDisposed = thenDisposed] {
            ++*thenCreated;
            flux::Reactive::onCleanup([thenDisposed] {
              ++*thenDisposed;
            });
            return flux::Element{flux::Rectangle{}}
                .size(20.f, 10.f)
                .fill(flux::Colors::red);
          },
          [elseCreated = elseCreated, elseDisposed = elseDisposed] {
            ++*elseCreated;
            flux::Reactive::onCleanup([elseDisposed] {
              ++*elseDisposed;
            });
            return flux::Element{flux::Rectangle{}}
                .size(12.f, 8.f)
                .fill(flux::Colors::blue);
          });
    }
  };

  int thenCreated = 0;
  int thenDisposed = 0;
  int elseCreated = 0;
  int elseDisposed = 0;
  flux::Reactive::Signal<bool> visible{true};
  FakeTextSystem textSystem;
  flux::scenegraph::SceneGraph sceneGraph;
  flux::MountRoot root{
      std::make_unique<flux::TypedRootHolder<Root>>(
          std::in_place, Root{visible, &thenCreated, &thenDisposed, &elseCreated, &elseDisposed}),
      textSystem,
      testEnvironment(),
      flux::Size{200.f, 100.f},
  };

  root.mount(sceneGraph);

  auto const& initial = rootGroup(sceneGraph);
  REQUIRE(initial.children().size() == 1);
  CHECK(thenCreated == 1);
  CHECK(thenDisposed == 0);
  CHECK(elseCreated == 0);
  CHECK(elseDisposed == 0);

  visible.set(false);

  auto const& hidden = rootGroup(sceneGraph);
  REQUIRE(hidden.children().size() == 1);
  CHECK(hidden.children()[0]->size().width == doctest::Approx(12.f));
  CHECK(thenCreated == 1);
  CHECK(thenDisposed == 1);
  CHECK(elseCreated == 1);
  CHECK(elseDisposed == 0);

  visible.set(true);

  auto const& shownAgain = rootGroup(sceneGraph);
  REQUIRE(shownAgain.children().size() == 1);
  CHECK(thenCreated == 2);
  CHECK(thenDisposed == 1);
  CHECK(elseCreated == 1);
  CHECK(elseDisposed == 1);

  root.unmount(sceneGraph);
  CHECK(thenDisposed == 2);
}

TEST_CASE("Show false branch collapses out of stack spacing") {
  struct Root {
    flux::Element body() const {
      return flux::VStack{
          .spacing = 12.f,
          .children = flux::children(
              flux::Element{flux::Rectangle{}}
                  .size(20.f, 10.f)
                  .fill(flux::Colors::red),
              flux::Show(false, [] {
                return flux::Element{flux::Rectangle{}}
                    .size(20.f, 10.f)
                    .fill(flux::Colors::blue);
              })),
      };
    }
  };

  FakeTextSystem textSystem;
  flux::scenegraph::SceneGraph sceneGraph;
  flux::MountRoot root{
      std::make_unique<flux::TypedRootHolder<Root>>(std::in_place, Root{}),
      textSystem,
      testEnvironment(),
      flux::Size{100.f, 100.f},
  };

  root.mount(sceneGraph);

  auto const& group = rootGroup(sceneGraph);
  REQUIRE(group.children().size() == 1);
}

TEST_CASE("Switch replaces scopes when the selected case changes") {
  struct Root {
    flux::Reactive::Signal<int> mode;
    int* created = nullptr;
    int* disposed = nullptr;

    flux::Element body() const {
      auto branch = [created = created, disposed = disposed](flux::Color color) {
        return [created, disposed, color] {
          ++*created;
          flux::Reactive::onCleanup([disposed] {
            ++*disposed;
          });
          return flux::Element{flux::Rectangle{}}
              .size(18.f, 18.f)
              .fill(color);
        };
      };

      return flux::Switch(
          [mode = mode] { return mode.get(); },
          std::vector{
              flux::Case(0, branch(flux::Colors::red)),
              flux::Case(1, branch(flux::Colors::green)),
          },
          branch(flux::Colors::blue));
    }
  };

  int created = 0;
  int disposed = 0;
  flux::Reactive::Signal<int> mode{0};
  FakeTextSystem textSystem;
  flux::scenegraph::SceneGraph sceneGraph;
  flux::MountRoot root{
      std::make_unique<flux::TypedRootHolder<Root>>(std::in_place, Root{mode, &created, &disposed}),
      textSystem,
      testEnvironment(),
      flux::Size{160.f, 100.f},
  };

  root.mount(sceneGraph);

  REQUIRE(rootGroup(sceneGraph).children().size() == 1);
  CHECK(created == 1);
  CHECK(disposed == 0);

  mode.set(1);
  REQUIRE(rootGroup(sceneGraph).children().size() == 1);
  CHECK(created == 2);
  CHECK(disposed == 1);

  mode.set(2);
  REQUIRE(rootGroup(sceneGraph).children().size() == 1);
  CHECK(created == 3);
  CHECK(disposed == 2);

  mode.set(3);
  REQUIRE(rootGroup(sceneGraph).children().size() == 1);
  CHECK(created == 3);
  CHECK(disposed == 2);

  root.unmount(sceneGraph);
  CHECK(disposed == 3);
}
