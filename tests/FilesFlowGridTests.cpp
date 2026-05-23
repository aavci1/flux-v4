#include <doctest/doctest.h>

#include <Flux/UI/Detail/RootHolder.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/Reactive/Signal.hpp>
#include <Flux/SceneGraph/SceneInteraction.hpp>
#include <Flux/SceneGraph/SceneNode.hpp>
#include <Flux/SceneGraph/RectNode.hpp>
#include <Flux/SceneGraph/SceneGraph.hpp>
#include <Flux/UI/InteractionData.hpp>
#include <Flux/UI/MeasureContext.hpp>
#include <Flux/UI/MountRoot.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/ScrollView.hpp>
#include <Flux/UI/Views/Show.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/VStack.hpp>

#include "FilesFlowGrid.hpp"
#include "FilesFlowGridLayout.hpp"
#include "FilesStore.hpp"
#include "FilesTheme.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace {

using namespace flux;
using namespace lambda_files;

class FakeTextSystem final : public TextSystem {
public:
  std::shared_ptr<TextLayout const> layout(AttributedString const&, float,
                                         TextLayoutOptions const&) override {
    return std::make_shared<TextLayout>();
  }

  std::shared_ptr<TextLayout const> layout(std::string_view, Font const&, Color const&, float,
                                         TextLayoutOptions const&) override {
    return std::make_shared<TextLayout>();
  }

  Size measure(AttributedString const&, float, TextLayoutOptions const&) override {
    return {0.f, 0.f};
  }

  Size measure(std::string_view text, Font const&, Color const&, float width,
               TextLayoutOptions const&) override {
  (void)text;
  (void)width;
    return {48.f, 14.f};
  }

  std::uint32_t resolveFontId(std::string_view, float, bool) override { return 0; }

  std::vector<std::uint8_t> rasterizeGlyph(std::uint32_t, std::uint32_t, float,
                                           std::uint32_t& outWidth, std::uint32_t& outHeight,
                                           Point& outBearing) override {
    outWidth = 0;
    outHeight = 0;
    outBearing = {};
    return {};
  }
};

EnvironmentBinding testEnvironment() {
  return EnvironmentBinding{}.withValue<ThemeKey>(Theme::light());
}

std::vector<FileEntry> makeEntries(int count, std::string const& directory = "/test/dir") {
  std::vector<FileEntry> entries;
  entries.reserve(static_cast<std::size_t>(count));
  for (int index = 0; index < count; ++index) {
    FileEntry entry;
    entry.name = "item-" + std::to_string(index);
    entry.path = std::filesystem::path(directory) / entry.name;
    entry.isDirectory = (index % 7) == 0;
    entries.push_back(std::move(entry));
  }
  return entries;
}

float constexpr kGridWidth = 780.f;

FilesFlowGridLayout const kLayout{};

scenegraph::SceneNode const* findClippingViewport(scenegraph::SceneNode const& node) {
  if (node.kind() == scenegraph::SceneNodeKind::Rect) {
    auto const& rect = static_cast<scenegraph::RectNode const&>(node);
    if (rect.clipsContents() && rect.bounds().height > 1.f) {
      return &node;
    }
  }
  for (auto const& child : node.children()) {
    if (child) {
      if (scenegraph::SceneNode const* found = findClippingViewport(*child)) {
        return found;
      }
    }
  }
  return nullptr;
}

scenegraph::SceneNode* findClippingViewport(scenegraph::SceneNode& node) {
  return const_cast<scenegraph::SceneNode*>(
      findClippingViewport(static_cast<scenegraph::SceneNode const&>(node)));
}

scenegraph::SceneNode const* scrollContentGroup(scenegraph::SceneNode const& viewport) {
  REQUIRE(viewport.children().size() >= 1);
  return viewport.children()[0].get();
}

scenegraph::SceneNode* scrollContentGroup(scenegraph::SceneNode& viewport) {
  REQUIRE(viewport.children().size() >= 1);
  return viewport.children()[0].get();
}

std::size_t gridRowCount(scenegraph::SceneNode const& gridRoot) {
  REQUIRE(gridRoot.children().size() == 1);
  scenegraph::SceneNode const& forGroup = *gridRoot.children()[0];
  return forGroup.children().size();
}

} // namespace

TEST_CASE("FilesFlowGrid layout column and row math") {
  CHECK(kLayout.columnCountForWidth(kGridWidth) == 6);
  CHECK(kLayout.rowCountForEntries(0, kGridWidth) == 0);
  CHECK(kLayout.rowCountForEntries(1, kGridWidth) == 1);
  CHECK(kLayout.rowCountForEntries(6, kGridWidth) == 1);
  CHECK(kLayout.rowCountForEntries(7, kGridWidth) == 2);
  CHECK(kLayout.rowCountForEntries(52, kGridWidth) == 9);

  Size const size52 = kLayout.contentSizeFor(kGridWidth, 52);
  CHECK(size52.width == doctest::Approx(kGridWidth));
  CHECK(size52.height ==
        doctest::Approx(9.f * FilesTheme::kGridTileH + 8.f * FilesTheme::kGridGapV));

  Size const size5 = kLayout.contentSizeFor(kGridWidth, 5);
  CHECK(size5.height == doctest::Approx(FilesTheme::kGridTileH));
  CHECK(size5.height < size52.height);

  std::vector<std::size_t> const rows = kLayout.rowIndicesFor(52, kGridWidth);
  REQUIRE(rows.size() == 9);
  CHECK(rows.front() == 0);
  CHECK(rows.back() == 8);
}

TEST_CASE("FilesStore display names truncate on UTF-8 scalar boundaries") {
  std::string name = "1234567890123456";
  name += "\xC3\xA9";
  name += "-tail";

  std::string expected = "1234567890123456";
  expected += "\xC3\xA9";
  expected += "...";
  CHECK(gridDisplayName(name) == expected);

  std::string invalid = "bad";
  invalid.push_back(static_cast<char>(0xC3));
  invalid += "name";
  CHECK(gridDisplayName(invalid) == std::string("bad\xEF\xBF\xBDname"));
}

TEST_CASE("FilesFlowGrid measure matches layout formula") {
  FakeTextSystem textSystem;
  Reactive::Signal<std::vector<FileEntry>> entries{makeEntries(52)};
  Reactive::Signal<std::string> listingKey{"/test/dir"};
  Reactive::Signal<std::string> selectedPath{};

  FilesFlowGrid grid{
      .entries = entries,
      .listingKey = listingKey,
      .selectedPath = selectedPath,
  };

  LayoutConstraints constraints{
      .maxWidth = kGridWidth,
      .maxHeight = std::numeric_limits<float>::infinity(),
      .minWidth = 0.f,
      .minHeight = 0.f,
  };

  MeasureContext measureContext{textSystem, testEnvironment()};
  Size const measured = grid.measure(measureContext, constraints, {}, textSystem);
  Size const expected = kLayout.contentSizeFor(kGridWidth, 52);
  CHECK(measured.width == doctest::Approx(expected.width));
  CHECK(measured.height == doctest::Approx(expected.height));
}

TEST_CASE("FilesFlowGrid has no extent before it receives a real width") {
  FakeTextSystem textSystem;
  Reactive::Signal<std::vector<FileEntry>> entries{makeEntries(8)};
  Reactive::Signal<std::string> listingKey{"/test/dir"};
  Reactive::Signal<std::string> selectedPath{};

  FilesFlowGrid grid{
      .entries = entries,
      .listingKey = listingKey,
      .selectedPath = selectedPath,
  };

  LayoutConstraints constraints{
      .maxWidth = std::numeric_limits<float>::infinity(),
      .maxHeight = std::numeric_limits<float>::infinity(),
      .minWidth = 0.f,
      .minHeight = 0.f,
  };

  MeasureContext measureContext{textSystem, testEnvironment()};
  Size const measured = grid.measure(measureContext, constraints, {}, textSystem);

  CHECK(measured.width == doctest::Approx(0.f));
  CHECK(measured.height == doctest::Approx(0.f));
}

TEST_CASE("FilesFlowGrid mounted row count follows entry count") {
  struct Root {
    Reactive::Signal<std::vector<FileEntry>> entries;
    Reactive::Signal<std::string> listingKey;

    Element body() const {
      return Element{FilesFlowGrid{
          .entries = entries,
          .listingKey = listingKey,
          .selectedPath = Reactive::Signal<std::string>{},
      }};
    }
  };

  FakeTextSystem textSystem;
  Reactive::Signal<std::vector<FileEntry>> entries{makeEntries(52)};
  Reactive::Signal<std::string> listingKey{"/test/dir"};
  scenegraph::SceneGraph sceneGraph;
  MountRoot root{std::make_unique<TypedRootHolder<Root>>(std::in_place, Root{entries, listingKey}),
                 textSystem,
                 testEnvironment(),
                 Size{kGridWidth, 600.f}};

  root.mount(sceneGraph);
  REQUIRE(sceneGraph.root().children().size() == 1);
  CHECK(gridRowCount(sceneGraph.root()) == 9);

  entries.set(makeEntries(5, "/test/dir"));
  listingKey.set("/test/dir");

  root.resize(Size{kGridWidth, 600.f}, sceneGraph);
  CHECK(gridRowCount(sceneGraph.root()) == 1);

  entries.set(makeEntries(13, "/other"));
  listingKey.set("/other");

  root.resize(Size{kGridWidth, 600.f}, sceneGraph);
  CHECK(gridRowCount(sceneGraph.root()) == 3);
}

TEST_CASE("FilesFlowGrid expands in a flex scroll viewport and remains clickable") {
  struct Root {
    Reactive::Signal<std::vector<FileEntry>> entries;
    Reactive::Signal<std::string> listingKey;
    std::shared_ptr<int> activations;

    Element body() const {
      return HStack{
          .spacing = 0.f,
          .alignment = Alignment::Stretch,
          .children = children(
              Rectangle{}.width(220.f),
              Element{ScrollView{
                  .axis = ScrollAxis::Vertical,
                  .children = children(Element{FilesFlowGrid{
                      .entries = entries,
                      .listingKey = listingKey,
                      .selectedPath = Reactive::Signal<std::string>{},
                      .activateEntry = [activations = activations](FileEntry const&) {
                        ++*activations;
                      },
                  }}),
              }}.flex(1.f, 1.f, 0.f)),
      };
    }
  };

  FakeTextSystem textSystem;
  Reactive::Signal<std::vector<FileEntry>> entries{makeEntries(10)};
  Reactive::Signal<std::string> listingKey{"/test/dir"};
  auto activations = std::make_shared<int>(0);
  scenegraph::SceneGraph sceneGraph;
  MountRoot root{std::make_unique<TypedRootHolder<Root>>(
                     std::in_place, Root{entries, listingKey, activations}),
                 textSystem,
                 testEnvironment(),
                 Size{820.f, 320.f}};

  root.mount(sceneGraph);
  root.resize(Size{820.f, 320.f}, sceneGraph);

  scenegraph::SceneNode const* viewport = findClippingViewport(sceneGraph.root());
  REQUIRE(viewport != nullptr);
  scenegraph::SceneNode const* content = scrollContentGroup(*viewport);
  REQUIRE(content->children().size() == 1);
  scenegraph::SceneNode const& gridRoot = *content->children()[0];
  CHECK(gridRoot.size().width == doctest::Approx(600.f).epsilon(0.01f));
  CHECK(gridRowCount(gridRoot) == kLayout.rowCountForEntries(10, 600.f));
  CHECK(gridRowCount(gridRoot) > 1);

  std::optional<scenegraph::InteractionHitResult> hit =
      scenegraph::hitTestInteraction(sceneGraph, Point{230.f, 20.f}, [](scenegraph::Interaction const& interaction) {
        return static_cast<bool>(interactionData(interaction).onTap);
      });
  REQUIRE(hit.has_value());
  interactionData(*hit->interaction).onTap(MouseButton::Left);
  CHECK(*activations == 1);
}

TEST_CASE("FilesFlowGrid expands inside the Files app shell layout") {
  struct Root {
    Reactive::Signal<std::vector<FileEntry>> entries;
    Reactive::Signal<std::string> listingKey;

    Element body() const {
      return VStack{
          .spacing = 0.f,
          .alignment = Alignment::Stretch,
          .children = children(
              Rectangle{}.height(FilesTheme::kTitlebarHeight),
              HStack{
                  .spacing = 0.f,
                  .alignment = Alignment::Stretch,
                  .children = children(
                      Rectangle{}.width(FilesTheme::kSidebarWidth),
                      Rectangle{}.width(1.f),
                      Element{ScrollView{
                          .axis = ScrollAxis::Vertical,
                          .children = children(Show(
                              [] { return false; },
                              [] {
                                return Text{.text = "error"};
                              },
                              [entries = entries, listingKey = listingKey] {
                                return Element{FilesFlowGrid{
                                    .entries = entries,
                                    .listingKey = listingKey,
                                    .selectedPath = Reactive::Signal<std::string>{},
                                }}
                                    .padding(FilesTheme::kContentPadV, FilesTheme::kContentPadH,
                                             FilesTheme::kContentPadV, FilesTheme::kContentPadH);
                              })),
                      }}.flex(1.f, 1.f, 0.f)),
              }.flex(1.f, 1.f, 0.f),
              Rectangle{}.height(FilesTheme::kStatusbarHeight)),
      };
    }
  };

  FakeTextSystem textSystem;
  Reactive::Signal<std::vector<FileEntry>> entries{makeEntries(80)};
  Reactive::Signal<std::string> listingKey{"/test/dir"};
  scenegraph::SceneGraph sceneGraph;
  MountRoot root{std::make_unique<TypedRootHolder<Root>>(
                     std::in_place, Root{entries, listingKey}),
                 textSystem,
                 testEnvironment(),
                 Size{1040.f, 680.f}};

  root.mount(sceneGraph);

  scenegraph::SceneNode* viewport = findClippingViewport(sceneGraph.root());
  REQUIRE(viewport != nullptr);
  scenegraph::SceneNode* content = scrollContentGroup(*viewport);
  REQUIRE(content->children().size() == 1);
  scenegraph::SceneNode const& gridRoot = *content->children()[0];
  float const gridWidth = 1040.f - FilesTheme::kSidebarWidth - 1.f -
                          2.f * FilesTheme::kContentPadH;

  REQUIRE(gridRoot.children().size() == 1);
  scenegraph::SceneNode const& paddedGrid = *gridRoot.children()[0];

  CHECK(paddedGrid.size().width == doctest::Approx(1040.f - FilesTheme::kSidebarWidth - 1.f).epsilon(0.01f));
  REQUIRE(paddedGrid.children().size() == 1);
  scenegraph::SceneNode const& grid = *paddedGrid.children()[0];
  CHECK(grid.size().width == doctest::Approx(gridWidth).epsilon(0.01f));
  CHECK(kLayout.columnCountForWidth(grid.size().width) >= 6);
  CHECK(gridRowCount(grid) == kLayout.rowCountForEntries(80, gridWidth));
  REQUIRE(content->size().height > viewport->size().height);

  auto const* scrollInteraction = interactionData(*viewport);
  REQUIRE(scrollInteraction != nullptr);
  REQUIRE(scrollInteraction->onScroll);
  scrollInteraction->onScroll(Vec2{0.f, -120.f});

  CHECK(content->position().y == doctest::Approx(-120.f));

  root.resize(Size{820.f, 680.f}, sceneGraph);

  scenegraph::SceneNode* resizedViewport = findClippingViewport(sceneGraph.root());
  REQUIRE(resizedViewport != nullptr);
  scenegraph::SceneNode* resizedContent = scrollContentGroup(*resizedViewport);
  REQUIRE(resizedContent->children().size() == 1);
  scenegraph::SceneNode const& resizedGridRoot = *resizedContent->children()[0];
  float const resizedGridWidth = 820.f - FilesTheme::kSidebarWidth - 1.f -
                                 2.f * FilesTheme::kContentPadH;

  REQUIRE(resizedGridRoot.children().size() == 1);
  scenegraph::SceneNode const& resizedPaddedGrid = *resizedGridRoot.children()[0];
  CHECK(resizedPaddedGrid.size().width ==
        doctest::Approx(820.f - FilesTheme::kSidebarWidth - 1.f).epsilon(0.01f));
  REQUIRE(resizedPaddedGrid.children().size() == 1);
  scenegraph::SceneNode const& resizedGrid = *resizedPaddedGrid.children()[0];
  CHECK(resizedGrid.size().width == doctest::Approx(resizedGridWidth).epsilon(0.01f));
  CHECK(kLayout.columnCountForWidth(resizedGrid.size().width) == 4);
  CHECK(gridRowCount(resizedGrid) == kLayout.rowCountForEntries(80, resizedGridWidth));
}

TEST_CASE("FilesFlowGrid ScrollView content size tracks listing changes") {
  struct Root {
    Reactive::Signal<std::vector<FileEntry>> entries;
    Reactive::Signal<std::string> listingKey;
    Reactive::Signal<Size> contentSize;
    Reactive::Signal<Size> viewportSize;

    Element body() const {
      return ScrollView{
          .axis = ScrollAxis::Vertical,
          .viewportSize = viewportSize,
          .contentSize = contentSize,
          .children = children(Element{FilesFlowGrid{
              .entries = entries,
              .listingKey = listingKey,
              .selectedPath = Reactive::Signal<std::string>{},
          }}),
      };
    }
  };

  FakeTextSystem textSystem;
  Reactive::Signal<std::vector<FileEntry>> entries{makeEntries(52)};
  Reactive::Signal<std::string> listingKey{"/test/dir"};
  Reactive::Signal<Size> contentSize{};
  Reactive::Signal<Size> viewportSize{};
  scenegraph::SceneGraph sceneGraph;
  MountRoot root{std::make_unique<TypedRootHolder<Root>>(
                     std::in_place,
                     Root{.entries = entries,
                          .listingKey = listingKey,
                          .contentSize = contentSize,
                          .viewportSize = viewportSize}),
                 textSystem,
                 testEnvironment(),
                 Size{kGridWidth, 320.f}};

  root.mount(sceneGraph);

  float const largeHeight = kLayout.contentSizeFor(kGridWidth, 52).height;
  float const smallHeight = kLayout.contentSizeFor(kGridWidth, 5).height;
  float const mediumHeight = kLayout.contentSizeFor(kGridWidth, 13).height;

  REQUIRE(largeHeight > mediumHeight);
  REQUIRE(mediumHeight > smallHeight);

  CHECK(contentSize.peek().height == doctest::Approx(largeHeight).epsilon(1.f));
  CHECK(viewportSize.peek().height == doctest::Approx(320.f).epsilon(1.f));
  CHECK(contentSize.peek().height > viewportSize.peek().height);

  scenegraph::SceneNode const* viewport = findClippingViewport(sceneGraph.root());
  REQUIRE(viewport != nullptr);
  scenegraph::SceneNode const* content = scrollContentGroup(*viewport);
  CHECK(content->size().height == doctest::Approx(largeHeight).epsilon(1.f));

  entries.set(makeEntries(5, "/test/dir"));
  listingKey.set("/test/dir");
  root.resize(Size{kGridWidth, 320.f}, sceneGraph);

  CHECK(contentSize.peek().height == doctest::Approx(smallHeight).epsilon(1.f));
  CHECK(content->size().height == doctest::Approx(smallHeight).epsilon(1.f));
  CHECK(contentSize.peek().height <= viewportSize.peek().height + 1.f);

  entries.set(makeEntries(13, "/other"));
  listingKey.set("/other");
  root.resize(Size{kGridWidth, 320.f}, sceneGraph);

  CHECK(contentSize.peek().height == doctest::Approx(mediumHeight).epsilon(1.f));
  CHECK(content->size().height == doctest::Approx(mediumHeight).epsilon(1.f));
  CHECK(mediumHeight == doctest::Approx(contentSize.peek().height).epsilon(1.f));
}

TEST_CASE("FilesFlowGrid ScrollView keeps child origins stable during scrolled content relayout") {
  struct Root {
    Reactive::Signal<std::vector<FileEntry>> entries;
    Reactive::Signal<std::string> listingKey;
    Reactive::Signal<Point> offset;

    Element body() const {
      return ScrollView{
          .axis = ScrollAxis::Vertical,
          .scrollOffset = offset,
          .children = children(Element{FilesFlowGrid{
              .entries = entries,
              .listingKey = listingKey,
              .selectedPath = Reactive::Signal<std::string>{},
          }}),
      };
    }
  };

  FakeTextSystem textSystem;
  Reactive::Signal<std::vector<FileEntry>> entries{makeEntries(52)};
  Reactive::Signal<std::string> listingKey{"/test/dir"};
  Reactive::Signal<Point> offset{Point{0.f, 40.f}};
  scenegraph::SceneGraph sceneGraph;
  MountRoot root{std::make_unique<TypedRootHolder<Root>>(
                     std::in_place, Root{entries, listingKey, offset}),
                 textSystem,
                 testEnvironment(),
                 Size{kGridWidth, 320.f}};

  root.mount(sceneGraph);

  scenegraph::SceneNode* viewport = findClippingViewport(sceneGraph.root());
  REQUIRE(viewport != nullptr);
  scenegraph::SceneNode* content = scrollContentGroup(*viewport);
  REQUIRE(content->children().size() == 1);
  CHECK(content->position().y == doctest::Approx(-40.f));
  CHECK(content->children()[0]->position().y == doctest::Approx(0.f));

  entries.set(makeEntries(13, "/other"));
  listingKey.set("/other");
  REQUIRE(content->relayoutStoredConstraints());

  CHECK(content->position().y == doctest::Approx(-40.f));
  REQUIRE(content->children().size() == 1);
  CHECK(content->children()[0]->position().y == doctest::Approx(0.f));
  CHECK(offset.get().y == doctest::Approx(40.f));
}

TEST_CASE("FilesFlowGrid ScrollView clamps offset when listing shrinks without a root resize") {
  struct Root {
    Reactive::Signal<std::vector<FileEntry>> entries;
    Reactive::Signal<std::string> listingKey;
    Reactive::Signal<Point> offset;
    Reactive::Signal<Size> contentSize;
    Reactive::Signal<Size> viewportSize;

    Element body() const {
      return ScrollView{
          .axis = ScrollAxis::Vertical,
          .scrollOffset = offset,
          .viewportSize = viewportSize,
          .contentSize = contentSize,
          .children = children(Element{FilesFlowGrid{
              .entries = entries,
              .listingKey = listingKey,
              .selectedPath = Reactive::Signal<std::string>{},
          }}),
      };
    }
  };

  FakeTextSystem textSystem;
  Reactive::Signal<std::vector<FileEntry>> entries{makeEntries(52)};
  Reactive::Signal<std::string> listingKey{"/test/dir"};
  Reactive::Signal<Point> offset{Point{0.f, 500.f}};
  Reactive::Signal<Size> contentSize{};
  Reactive::Signal<Size> viewportSize{};
  scenegraph::SceneGraph sceneGraph;
  MountRoot root{std::make_unique<TypedRootHolder<Root>>(
                     std::in_place, Root{entries, listingKey, offset, contentSize, viewportSize}),
                 textSystem,
                 testEnvironment(),
                 Size{kGridWidth, 320.f}};

  root.mount(sceneGraph);
  REQUIRE(contentSize.peek().height > viewportSize.peek().height);
  REQUIRE(offset.peek().y > 0.f);

  entries.set(makeEntries(5, "/test/dir"));
  listingKey.set("/test/dir");

  CHECK(contentSize.peek().height == doctest::Approx(FilesTheme::kGridTileH).epsilon(1.f));
  CHECK(contentSize.peek().height <= viewportSize.peek().height + 1.f);
  CHECK(offset.peek().y == doctest::Approx(0.f));

  entries.set(makeEntries(52, "/test/dir"));
  listingKey.set("/test/dir");

  CHECK(contentSize.peek().height == doctest::Approx(kLayout.contentSizeFor(kGridWidth, 52).height).epsilon(1.f));
  CHECK(offset.peek().y == doctest::Approx(0.f));
}

TEST_CASE("FilesFlowGrid ScrollView drops stale content height after shrink") {
  struct Root {
    Reactive::Signal<std::vector<FileEntry>> entries;
    Reactive::Signal<std::string> listingKey;
    Reactive::Signal<Size> contentSize;

    Element body() const {
      return ScrollView{
          .axis = ScrollAxis::Vertical,
          .contentSize = contentSize,
          .children = children(Element{FilesFlowGrid{
              .entries = entries,
              .listingKey = listingKey,
              .selectedPath = Reactive::Signal<std::string>{},
          }}),
      };
    }
  };

  FakeTextSystem textSystem;
  Reactive::Signal<std::vector<FileEntry>> entries{makeEntries(52)};
  Reactive::Signal<std::string> listingKey{"/test/dir"};
  Reactive::Signal<Size> contentSize{};
  scenegraph::SceneGraph sceneGraph;
  MountRoot root{std::make_unique<TypedRootHolder<Root>>(
                     std::in_place, Root{entries, listingKey, contentSize}),
                 textSystem,
                 testEnvironment(),
                 Size{kGridWidth, 400.f}};

  root.mount(sceneGraph);
  float const largeHeight = contentSize.peek().height;

  entries.set(makeEntries(5, "/test/dir"));
  listingKey.set("/test/dir");
  root.resize(Size{kGridWidth, 400.f}, sceneGraph);

  float const smallHeight = contentSize.peek().height;
  CHECK(smallHeight < largeHeight * 0.5f);

  scenegraph::SceneNode const* viewport = findClippingViewport(sceneGraph.root());
  REQUIRE(viewport != nullptr);
  float const maxScroll = std::max(0.f, scrollContentGroup(*viewport)->size().height -
                                            viewport->size().height);
  CHECK(maxScroll < largeHeight * 0.5f);
}
