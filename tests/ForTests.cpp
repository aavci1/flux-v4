#include <doctest/doctest.h>

#include <Flux/Detail/RootHolder.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/Reactive/Scope.hpp>
#include <Flux/Reactive/Signal.hpp>
#include <Flux/SceneGraph/GroupNode.hpp>
#include <Flux/SceneGraph/RectNode.hpp>
#include <Flux/SceneGraph/SceneGraph.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/MountRoot.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/For.hpp>
#include <Flux/UI/Views/Rectangle.hpp>

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

TEST_CASE("For preserves row scopes and scene nodes across reorder") {
  struct Root {
    flux::Reactive::Signal<std::vector<int>> items;
    int* created = nullptr;
    int* disposed = nullptr;

    flux::Element body() const {
      return flux::For(
          items,
          [](int value) { return value; },
          [created = created, disposed = disposed](int value,
                                                   flux::Reactive::Signal<std::size_t> index) {
            auto local = flux::useState(value * 10);
            (void)local;
            ++*created;
            flux::Reactive::onCleanup([disposed] {
              ++*disposed;
            });

            flux::Reactive::Bindable<float> width{[index] {
              return 20.f + static_cast<float>(index.get());
            }};
            return flux::Element{flux::Rectangle{}}
                .size(std::move(width), flux::Reactive::Bindable<float>{8.f})
                .fill(flux::Colors::blue);
          },
          2.f);
    }
  };

  int created = 0;
  int disposed = 0;
  flux::Reactive::Signal<std::vector<int>> items{{1, 2, 3}};
  FakeTextSystem textSystem;
  flux::scenegraph::SceneGraph sceneGraph;
  flux::MountRoot root{
      std::make_unique<flux::TypedRootHolder<Root>>(std::in_place, Root{items, &created, &disposed}),
      textSystem,
      testEnvironment(),
      flux::Size{240.f, 160.f},
  };

  root.mount(sceneGraph);

  auto const& initial = rootGroup(sceneGraph);
  REQUIRE(initial.children().size() == 3);
  CHECK(created == 3);
  CHECK(disposed == 0);
  auto* first = initial.children()[0].get();
  auto* second = initial.children()[1].get();
  auto* third = initial.children()[2].get();

  items.set({3, 2, 1});

  auto const& reordered = rootGroup(sceneGraph);
  REQUIRE(reordered.children().size() == 3);
  CHECK(created == 3);
  CHECK(disposed == 0);
  CHECK(reordered.children()[0].get() == third);
  CHECK(reordered.children()[1].get() == second);
  CHECK(reordered.children()[2].get() == first);
  CHECK(reordered.children()[0]->size().width == doctest::Approx(20.f));
  CHECK(reordered.children()[1]->size().width == doctest::Approx(21.f));
  CHECK(reordered.children()[2]->size().width == doctest::Approx(22.f));

  items.set({3, 1});

  auto const& removed = rootGroup(sceneGraph);
  REQUIRE(removed.children().size() == 2);
  CHECK(created == 3);
  CHECK(disposed == 1);
  CHECK(removed.children()[0].get() == third);
  CHECK(removed.children()[1].get() == first);

  items.set({4, 5});

  auto const& replaced = rootGroup(sceneGraph);
  REQUIRE(replaced.children().size() == 2);
  CHECK(created == 5);
  CHECK(disposed == 3);

  root.unmount(sceneGraph);
  CHECK(disposed == 5);
}
