#pragma once

#include <doctest/doctest.h>

#include <Flux/Core/Application.hpp>
#include <Flux/Detail/RootHolder.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/Reactive/Signal.hpp>
#include <Flux/Scene/CustomTransformSceneNode.hpp>
#include <Flux/Scene/HitTester.hpp>
#include <Flux/Scene/PathSceneNode.hpp>
#include <Flux/Scene/RectSceneNode.hpp>
#include <Flux/Scene/ModifierSceneNode.hpp>
#include <Flux/Scene/Render.hpp>
#include <Flux/Scene/Renderer.hpp>
#include <Flux/Scene/SceneTree.hpp>
#include <Flux/Scene/SceneTreeInteraction.hpp>
#include <Flux/Scene/TextSceneNode.hpp>
#include <Flux/UI/Environment.hpp>
#include <Flux/UI/FocusController.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/GestureTracker.hpp>
#include <Flux/UI/LayoutEngine.hpp>
#include <Flux/UI/SceneBuilder.hpp>
#include <Flux/UI/SceneGeometryIndex.hpp>
#include <Flux/UI/StateStore.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/ScrollView.hpp>
#include <Flux/UI/Views/Grid.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/PopoverCalloutShape.hpp>
#include <Flux/UI/Views/ScaleAroundCenter.hpp>
#include <Flux/UI/Views/Spacer.hpp>
#include <Flux/UI/Views/Button.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/VStack.hpp>
#include <Flux/UI/Views/ZStack.hpp>

#include <memory>
#include <chrono>
#include <functional>
#include <string_view>
#include <thread>
#include <typeindex>

namespace {

using namespace flux;

class NullTextSystem : public TextSystem {
public:
  std::shared_ptr<TextLayout const> layout(AttributedString const&, float, TextLayoutOptions const&) override {
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

  Size measure(std::string_view, Font const&, Color const&, float, TextLayoutOptions const&) override {
    return Size{48.f, 14.f};
  }

  std::uint32_t resolveFontId(std::string_view, float, bool) override { return 0; }

  std::vector<std::uint8_t> rasterizeGlyph(std::uint32_t, std::uint16_t, float, std::uint32_t&,
                                           std::uint32_t&, Point&) override {
    return {};
  }
};

class CountingTextSystem final : public NullTextSystem {
public:
  int layoutCount = 0;

  std::shared_ptr<TextLayout const> layout(AttributedString const& text, float maxWidth,
                                           TextLayoutOptions const& options) override {
    ++layoutCount;
    return NullTextSystem::layout(text, maxWidth, options);
  }

  std::shared_ptr<TextLayout const> layout(std::string_view text, Font const& font, Color const& color,
                                           float maxWidth, TextLayoutOptions const& options) override {
    ++layoutCount;
    return NullTextSystem::layout(text, font, color, maxWidth, options);
  }
};

class VariableTextSystem final : public TextSystem {
public:
  std::shared_ptr<TextLayout const> layout(AttributedString const& text, float maxWidth,
                                           TextLayoutOptions const&) override {
    auto out = std::make_shared<TextLayout>();
    out->measuredSize = measuredSizeForLength(text.utf8.size(), maxWidth);
    return out;
  }

  std::shared_ptr<TextLayout const> layout(std::string_view text, Font const&, Color const&, float maxWidth,
                                           TextLayoutOptions const&) override {
    auto out = std::make_shared<TextLayout>();
    out->measuredSize = measuredSizeForLength(text.size(), maxWidth);
    return out;
  }

  Size measure(AttributedString const& text, float maxWidth, TextLayoutOptions const&) override {
    return measuredSizeForLength(text.utf8.size(), maxWidth);
  }

  Size measure(std::string_view text, Font const&, Color const&, float maxWidth,
               TextLayoutOptions const&) override {
    return measuredSizeForLength(text.size(), maxWidth);
  }

  std::uint32_t resolveFontId(std::string_view, float, bool) override { return 0; }

  std::vector<std::uint8_t> rasterizeGlyph(std::uint32_t, std::uint16_t, float, std::uint32_t&,
                                           std::uint32_t&, Point&) override {
    return {};
  }

private:
  static Size measuredSizeForLength(std::size_t length, float maxWidth) {
    float const intrinsicWidth = std::max(1.f, 7.f * static_cast<float>(length));
    if (maxWidth > 0.f) {
      float const lineWidth = std::min(intrinsicWidth, maxWidth);
      float const lines = std::max(1.f, std::ceil(intrinsicWidth / maxWidth));
      return Size{lineWidth, 14.f * lines};
    }
    return Size{intrinsicWidth, 14.f};
  }
};

class NullRenderer final : public Renderer {
public:
  int rectCount = 0;
  int textCount = 0;
  int pathCount = 0;
  std::vector<Rect> rects;
  std::vector<Point> textOrigins;

  void save() override {}
  void restore() override {}
  void translate(Point) override {}
  void transform(Mat3 const&) override {}
  void clipRect(Rect, CornerRadius const& = CornerRadius{}, bool = false) override {}
  bool quickReject(Rect) const override { return false; }
  void setOpacity(float) override {}
  void setBlendMode(BlendMode) override {}
  void drawRect(Rect const& rect, CornerRadius const&, FillStyle const&, StrokeStyle const&, ShadowStyle const&) override {
    ++rectCount;
    rects.push_back(rect);
  }
  void drawPath(Path const&, FillStyle const&, StrokeStyle const&, ShadowStyle const&) override { ++pathCount; }
  void drawTextLayout(TextLayout const&, Point origin) override {
    ++textCount;
    textOrigins.push_back(origin);
  }
  void drawImage(Image const&, Rect const&, ImageFillMode, CornerRadius const&, float) override {}
};

struct EnvironmentScope {
  explicit EnvironmentScope(EnvironmentLayer layer) {
    EnvironmentStack::current().push(std::move(layer));
  }
  ~EnvironmentScope() { EnvironmentStack::current().pop(); }
};

struct StoreScope {
  flux::StateStore store;
  flux::StateStore* previous = nullptr;

  StoreScope() {
    previous = flux::StateStore::current();
    flux::StateStore::setCurrent(&store);
  }

  ~StoreScope() { flux::StateStore::setCurrent(previous); }
};

struct ScrollSizedComposite {
  std::string key;
  float width = 0.f;
  float height = 0.f;

  Element body() const {
    return Element{Rectangle{}}
        .key(key)
        .size(width, height);
  }
};

struct CountingMeasureLeaf : ViewModifiers<CountingMeasureLeaf> {
  int* measureCalls = nullptr;
  float width = 0.f;
  float height = 0.f;

  Size measure(MeasureContext& ctx, LayoutConstraints const&, LayoutHints const&, TextSystem&) const {
    ++*measureCalls;
    ctx.advanceChildSlot();
    return Size{width, height};
  }
};

} // namespace

namespace {

Element keyedRect(std::string key, float width, float height) {
  return Element{Rectangle{}}.key(std::move(key)).size(width, height);
}

Element demoColorBlock(float width, float height) {
  return Element{Rectangle{}}.size(width, height);
}

Element demoJustifyPreviewBlock(float width, float height) {
  return Element{Rectangle{}}.size(width, height);
}

Element demoJustifySetting(std::string label, std::string value) {
  return VStack{
      .spacing = 4.f,
      .alignment = Alignment::Stretch,
      .children = children(
          Text{
              .text = std::move(label),
              .horizontalAlignment = HorizontalAlignment::Leading,
          },
          Text{
              .text = std::move(value),
              .horizontalAlignment = HorizontalAlignment::Leading,
          }
              .padding(10.f, 12.f, 10.f, 12.f)
              .fill(FillStyle::solid(Color::hex(0xF7F7F7)))
              .cornerRadius(CornerRadius{8.f}))
  };
}

Element demoHStackJustifyPlaygroundPreview(Alignment alignment, JustifyContent justifyContent) {
  return HStack{
      .spacing = 8.f,
      .alignment = alignment,
      .justifyContent = justifyContent,
      .children = children(
          demoJustifyPreviewBlock(40.f, 44.f),
          demoJustifyPreviewBlock(56.f, 88.f),
          demoJustifyPreviewBlock(32.f, 60.f))
  }
      .size(0.f, 152.f)
      .padding(12.f)
      .fill(FillStyle::solid(Color::hex(0xF7F7F7)))
      .cornerRadius(CornerRadius{8.f});
}

Element demoVStackJustifyPlaygroundPreview(Alignment alignment, JustifyContent justifyContent) {
  return HStack{
      .alignment = Alignment::Center,
      .children = children(
          Spacer{},
          VStack{
              .spacing = 8.f,
              .alignment = alignment,
              .justifyContent = justifyContent,
              .children = children(
                  demoJustifyPreviewBlock(88.f, 32.f),
                  demoJustifyPreviewBlock(124.f, 44.f),
                  demoJustifyPreviewBlock(68.f, 36.f))
          }
              .size(160.f, 188.f)
              .padding(12.f)
              .fill(FillStyle::solid(Color::hex(0xF7F7F7)))
              .cornerRadius(CornerRadius{8.f}),
          Spacer{})
  }
      .size(0.f, 188.f);
}

Element demoSectionCard(std::string title, std::string caption, Element content) {
  return VStack{
      .spacing = 16.f,
      .children = children(
          Text{
              .text = std::move(title),
              .horizontalAlignment = HorizontalAlignment::Leading,
          },
          Text{
              .text = std::move(caption),
              .horizontalAlignment = HorizontalAlignment::Leading,
              .wrapping = TextWrapping::Wrap,
          },
          std::move(content)
      ),
  }
      .padding(16.f)
      .fill(FillStyle::solid(Color::hex(0xFFFFFF)))
      .stroke(StrokeStyle::solid(Color::hex(0xE0E0E0), 1.f))
      .cornerRadius(CornerRadius{12.f});
}

Element demoVStackCard() {
  return demoSectionCard(
      "VStack",
      "Children flow top-to-bottom. Center alignment keeps each child at its intrinsic width and centers it in the column.",
      VStack{
          .spacing = 12.f,
          .alignment = Alignment::Center,
          .children = children(
              demoColorBlock(160.f, 34.f),
              demoColorBlock(220.f, 42.f),
              demoColorBlock(120.f, 30.f),
              HStack{
                  .spacing = 8.f,
                  .alignment = Alignment::Center,
                  .children = children(
                      Text{
                          .text = "Rows can also host nested stacks",
                          .horizontalAlignment = HorizontalAlignment::Leading,
                      },
                      Spacer{},
                      Text{
                          .text = "nested",
                          .horizontalAlignment = HorizontalAlignment::Trailing,
                      }
                  )
              }
                  .padding(12.f)
                  .fill(FillStyle::solid(Color::hex(0xF7F7F7)))
                  .cornerRadius(CornerRadius{8.f})
          )
      }
          .padding(12.f)
          .fill(FillStyle::solid(Color::hex(0xFAFAFA)))
          .cornerRadius(CornerRadius{8.f}));
}

Element demoHStackCard() {
  return demoSectionCard(
      "HStack",
      "Children flow left-to-right. Flex growth lets selected items absorb remaining width.",
      VStack{
          .spacing = 12.f,
          .children = children(
              HStack{
                  .spacing = 12.f,
                  .alignment = Alignment::Center,
                  .children = children(
                      demoColorBlock(56.f, 54.f).flex(2.f, 1.f, 0.f),
                      demoColorBlock(56.f, 76.f),
                      demoColorBlock(56.f, 40.f).flex(1.f, 1.f, 0.f),
                      demoColorBlock(56.f, 54.f)
                  )
              },
              HStack{
                  .spacing = 0.f,
                  .alignment = Alignment::Center,
                  .children = children(
                      Text{.text = "Leading"},
                      Spacer{},
                      Text{
                          .text = "Spacer pushes this trailing label",
                          .horizontalAlignment = HorizontalAlignment::Trailing,
                      }
                  )
              }
                  .padding(12.f)
                  .fill(FillStyle::solid(Color::hex(0xF7F7F7)))
                  .cornerRadius(CornerRadius{8.f})
          )
      }
          .padding(12.f)
          .fill(FillStyle::solid(Color::hex(0xFAFAFA)))
          .cornerRadius(CornerRadius{8.f}));
}

Element demoZStackCard() {
  return demoSectionCard(
      "ZStack",
      "Children share the same space. This is useful for overlays, badges, and stacked decoration.",
      ZStack{
          .horizontalAlignment = Alignment::Center,
          .verticalAlignment = Alignment::Center,
          .children = children(
              Element{Rectangle{}}.size(0.f, 180.f),
              Element{Rectangle{}}.size(220.f, 104.f),
              Element{VStack{
                  .spacing = 4.f,
                  .alignment = Alignment::Center,
                  .justifyContent = JustifyContent::Center,
                  .children = children(
                      Text{
                          .text = "Overlay content",
                          .horizontalAlignment = HorizontalAlignment::Center,
                      },
                      Text{
                          .text = "Centered inside a shared layer",
                          .horizontalAlignment = HorizontalAlignment::Center,
                      }
                  )
              }}
          )
      }
          .padding(12.f)
          .fill(FillStyle::solid(Color::hex(0xFAFAFA)))
          .cornerRadius(CornerRadius{8.f}));
}

Element demoGridCard() {
  std::vector<Element> cells;
  cells.reserve(8);
  for (int i = 0; i < 8; ++i) {
    cells.push_back(
        VStack{
            .spacing = 4.f,
            .alignment = Alignment::Start,
            .children = children(
                Text{
                    .text = "Cell " + std::to_string(i + 1),
                    .horizontalAlignment = HorizontalAlignment::Leading,
                },
                demoColorBlock(24.f + static_cast<float>((i % 3) * 18), 18.f + static_cast<float>((i % 2) * 12))
            )
        }
            .padding(12.f)
            .fill(FillStyle::solid(Color::hex(0xF7F7F7)))
            .cornerRadius(CornerRadius{8.f}));
  }

  std::vector<Element> spanCells;
  spanCells.reserve(5);
  spanCells.push_back(
      HStack{
          .spacing = 12.f,
          .alignment = Alignment::Center,
          .children = children(
              VStack{
                  .spacing = 4.f,
                  .alignment = Alignment::Start,
                  .children = children(
                      Text{.text = "Span 3", .horizontalAlignment = HorizontalAlignment::Leading},
                      Text{
                          .text = "Full-width cells can reset the rhythm before smaller cells continue.",
                          .horizontalAlignment = HorizontalAlignment::Leading,
                          .wrapping = TextWrapping::Wrap,
                      })
              }
                  .flex(1.f, 1.f, 0.f),
              demoColorBlock(28.f, 28.f))
      }
          .padding(12.f)
          .fill(FillStyle::solid(Color::hex(0xE8F0FE)))
          .cornerRadius(CornerRadius{8.f}));
  spanCells.push_back(
      VStack{
          .spacing = 4.f,
          .alignment = Alignment::Start,
          .children = children(
              Text{.text = "Span 1", .horizontalAlignment = HorizontalAlignment::Leading},
              demoColorBlock(26.f, 18.f))
      }
          .padding(12.f)
          .fill(FillStyle::solid(Color::hex(0xF7F7F7)))
          .cornerRadius(CornerRadius{8.f}));
  spanCells.push_back(
      VStack{
          .spacing = 4.f,
          .alignment = Alignment::Start,
          .children = children(
              Text{.text = "Span 2", .horizontalAlignment = HorizontalAlignment::Leading},
              Text{
                  .text = "Wider detail cells stay readable without leaving the grid.",
                  .horizontalAlignment = HorizontalAlignment::Leading,
                  .wrapping = TextWrapping::Wrap,
              })
      }
          .padding(12.f)
          .fill(FillStyle::solid(Color::hex(0xE8F5E9)))
          .cornerRadius(CornerRadius{8.f}));
  spanCells.push_back(
      VStack{
          .spacing = 4.f,
          .alignment = Alignment::Start,
          .children = children(
              Text{.text = "Span 1", .horizontalAlignment = HorizontalAlignment::Leading},
              demoColorBlock(20.f, 20.f))
      }
          .padding(12.f)
          .fill(FillStyle::solid(Color::hex(0xFFF8E1)))
          .cornerRadius(CornerRadius{8.f}));
  spanCells.push_back(
      VStack{
          .spacing = 4.f,
          .alignment = Alignment::Start,
          .children = children(
              Text{.text = "Span 1", .horizontalAlignment = HorizontalAlignment::Leading},
              demoColorBlock(20.f, 20.f))
      }
          .padding(12.f)
          .fill(FillStyle::solid(Color::hex(0xFDECEA)))
          .cornerRadius(CornerRadius{8.f}));

  return demoSectionCard(
      "Grid",
      "Fixed columns place children row-by-row. Mixed intrinsic sizes stay aligned inside each cell, and column spans let specific items stretch across multiple tracks.",
      VStack{
          .spacing = 12.f,
          .alignment = Alignment::Stretch,
          .children = children(
              Grid{
                  .columns = 3,
                  .horizontalSpacing = 12.f,
                  .verticalSpacing = 12.f,
                  .horizontalAlignment = Alignment::Center,
                  .verticalAlignment = Alignment::Center,
                  .children = std::move(cells),
              },
              Grid{
                  .columns = 3,
                  .horizontalSpacing = 12.f,
                  .verticalSpacing = 12.f,
                  .horizontalAlignment = Alignment::Start,
                  .verticalAlignment = Alignment::Center,
                  .children = std::move(spanCells),
                  .columnSpans = {3u, 1u, 2u, 1u, 1u},
              })
      }
          .padding(12.f)
          .fill(FillStyle::solid(Color::hex(0xFAFAFA)))
          .cornerRadius(CornerRadius{8.f}));
}

Element demoMixedCompositionCard() {
  std::vector<Element> rows;
  rows.reserve(4);
  for (int i = 0; i < 4; ++i) {
    rows.push_back(
        HStack{
            .spacing = 12.f,
            .alignment = Alignment::Center,
            .children = children(
                Text{
                    .text = "Track " + std::to_string(i + 1),
                    .horizontalAlignment = HorizontalAlignment::Leading,
                }
                    .width(72.f),
                Element{Rectangle{}}
                    .height(14.f)
                    .flex(1.f, 1.f, 0.f),
                Text{
                    .text = i % 2 == 0 ? "auto" : "manual",
                    .horizontalAlignment = HorizontalAlignment::Trailing,
                })
        }
            .padding(8.f)
            .fill(FillStyle::solid(Color::hex(0xF7F7F7)))
            .cornerRadius(CornerRadius{8.f}));
  }

  return demoSectionCard(
      "Composed Layout",
      "Real views usually mix stacks together. This section shows a common label-track-value row pattern.",
      VStack{
          .spacing = 8.f,
          .alignment = Alignment::Stretch,
          .children = std::move(rows)
      }
          .padding(12.f)
          .fill(FillStyle::solid(Color::hex(0xFAFAFA)))
          .cornerRadius(CornerRadius{8.f}));
}

Element demoJustifyContentCard() {
  return demoSectionCard(
      "Justify Content",
      "The app demo uses selects to switch between HStack and VStack, cross-axis alignment, and flexbox-like justify-content behavior in one preview. The fixture keeps the default HStack + Center + SpaceAround state.",
      VStack{
          .spacing = 12.f,
          .alignment = Alignment::Stretch,
          .children = children(
              demoJustifySetting("Axis", "HStack"),
              demoJustifySetting("Alignment", "Center"),
              demoJustifySetting("Justify", "SpaceAround"),
              VStack{
                  .spacing = 8.f,
                  .alignment = Alignment::Stretch,
                  .children = children(
                      Text{
                          .text = "HStack using Center alignment and SpaceAround distribution.",
                          .horizontalAlignment = HorizontalAlignment::Leading,
                          .wrapping = TextWrapping::Wrap,
                      },
                      demoHStackJustifyPlaygroundPreview(Alignment::Center, JustifyContent::SpaceAround))
              }
                  .padding(12.f)
                  .fill(FillStyle::solid(Color::hex(0xFFFFFF)))
                  .cornerRadius(CornerRadius{8.f}))
      });
}

Element demoBasisChip(std::string text) {
  return Text{
      .text = std::move(text),
      .horizontalAlignment = HorizontalAlignment::Leading,
  }
      .padding(8.f, 12.f, 8.f, 12.f)
      .fill(FillStyle::solid(Color::hex(0xE8F0FE)))
      .cornerRadius(CornerRadius{8.f});
}

Element demoFlexBasisLane(std::string title, std::string caption,
                          std::function<Element(Element)> applyLeft,
                          std::function<Element(Element)> applyRight) {
  return VStack{
      .spacing = 8.f,
      .alignment = Alignment::Stretch,
      .children = children(
          Text{
              .text = std::move(title),
              .horizontalAlignment = HorizontalAlignment::Leading,
          },
          Text{
              .text = std::move(caption),
              .horizontalAlignment = HorizontalAlignment::Leading,
              .wrapping = TextWrapping::Wrap,
          },
          HStack{
              .spacing = 8.f,
              .alignment = Alignment::Stretch,
              .children = children(
                  applyLeft(demoBasisChip("Short")),
                  applyRight(demoBasisChip("A much wider content block")))
          }
              .size(0.f, 56.f)
              .padding(8.f)
              .fill(FillStyle::solid(Color::hex(0xF7F7F7)))
              .cornerRadius(CornerRadius{8.f}))
  };
}

Element demoFlexBasisCard() {
  return demoSectionCard(
      "Flex Basis",
      "Equal grow factors can either preserve intrinsic size or ignore it. `flex(1)` and `flex(1, 1)` both use an auto basis, while `flex(..., 0)` starts from zero.",
      VStack{
          .spacing = 12.f,
          .alignment = Alignment::Stretch,
          .children = children(
              demoFlexBasisLane(
                  "flex(1, 1)",
                  "Auto basis keeps the wider item wider before the extra width is shared.",
                  [](Element chip) { return std::move(chip).flex(1.f, 1.f); },
                  [](Element chip) { return std::move(chip).flex(1.f, 1.f); }),
              demoFlexBasisLane(
                  "flex(1)",
                  "The shorthand uses grow 1, shrink 1, and the same auto basis, so it matches the relative sizing above.",
                  [](Element chip) { return std::move(chip).flex(1.f); },
                  [](Element chip) { return std::move(chip).flex(1.f); }),
              demoFlexBasisLane(
                  "flex(1, 1, 0)",
                  "Zero basis ignores intrinsic width first, so equal grow factors produce equal columns.",
                  [](Element chip) { return std::move(chip).flex(1.f, 1.f, 0.f); },
                  [](Element chip) { return std::move(chip).flex(1.f, 1.f, 0.f); }))
      }
          .padding(12.f)
          .fill(FillStyle::solid(Color::hex(0xFAFAFA)))
          .cornerRadius(CornerRadius{8.f}));
}

Element demoLayoutRoot() {
  return ScrollView{
      .axis = ScrollAxis::Vertical,
      .children = children(
          VStack{
              .spacing = 16.f,
              .children = children(
                  Text{
                      .text = "Layout Demo",
                      .horizontalAlignment = HorizontalAlignment::Leading,
                  },
                  Text{
                      .text =
                          "Focused examples for stacks, grids, an interactive justify-content playground, flex-basis behavior, and how they compose in practice.",
                      .horizontalAlignment = HorizontalAlignment::Leading,
                      .wrapping = TextWrapping::Wrap,
                  },
                  demoVStackCard(),
                  demoHStackCard(),
                  demoZStackCard(),
                  demoGridCard(),
                  demoMixedCompositionCard(),
                  demoJustifyContentCard(),
                  demoFlexBasisCard()
              )
          }
              .padding(20.f)
      )
  }
      .fill(FillStyle::solid(Color::hex(0xFFFFFF)));
}

struct InteractiveRectTree {
  SceneTree tree;
  NodeId leafId{};
};

InteractiveRectTree makeInteractiveRectTree(std::string key, bool focusable = false,
                                            std::function<void()> onTap = {}) {
  auto root = std::make_unique<SceneNode>(NodeId{1ull});
  auto rect = std::make_unique<RectSceneNode>(NodeId{2ull});
  RectSceneNode* rectPtr = rect.get();
  rect->size = Size{40.f, 20.f};
  auto interaction = std::make_unique<InteractionData>();
  interaction->stableTargetKey = ComponentKey{LocalId::fromString(key)};
  interaction->focusable = focusable;
  interaction->onTap = std::move(onTap);
  rect->setInteraction(std::move(interaction));
  rect->recomputeBounds();
  root->appendChild(std::move(rect));
  root->recomputeBounds();
  return InteractiveRectTree{SceneTree{std::move(root)}, rectPtr->id()};
}

struct HelloRoot {
  Element body() const {
    return Text{
        .text = "Hello, World!",
    };
  }
};

struct RetainedTextChild {
  int* bodyCalls = nullptr;

  Element body() const {
    ++*bodyCalls;
    return Text{
        .text = "Retained child",
    };
  }
};

struct RetainedParent {
  State<int> tick{};
  int* parentCalls = nullptr;
  int* childCalls = nullptr;

  Element body() const {
    ++*parentCalls;
    return VStack{
        .spacing = static_cast<float>(*tick),
        .children = {
            Element{Rectangle{}}.size(24.f + static_cast<float>(*tick), 8.f),
            Element{RetainedTextChild{childCalls}}.key("child"),
        },
    };
  }
};

struct ThemeSensitiveChild {
  int* bodyCalls = nullptr;

  Element body() const {
    ++*bodyCalls;
    Theme const& theme = useEnvironment<Theme>();
    return Element{Rectangle{}}
        .size(24.f, 2.f)
        .fill(FillStyle::solid(theme.separatorColor));
  }
};

struct ThemeSensitiveParent {
  State<bool> dark{};
  int* parentCalls = nullptr;
  int* childCalls = nullptr;

  Element body() const {
    ++*parentCalls;
    Theme const theme = *dark ? Theme::dark() : Theme::light();
    return Element{VStack{
        .spacing = *dark ? 6.f : 0.f,
        .alignment = Alignment::Stretch,
        .children = {
            Element{Rectangle{}}.size(24.f, 8.f),
            Element{ThemeSensitiveChild{childCalls}}.key("child"),
        },
    }}.environment(theme);
  }
};

struct CompositeRootScrollView {
  Element body() const {
    auto hostState = useState(0);
    (void)hostState;
    return ScrollView{
        .axis = ScrollAxis::Vertical,
        .children = {
            keyedRect("a", 60.f, 30.f),
            keyedRect("b", 60.f, 30.f),
        },
    };
  }
};

struct CompositeRootModifierShell {
  bool* tapped = nullptr;

  Element body() const {
    return HStack{
        .spacing = 8.f,
        .alignment = Alignment::Center,
        .children = children(
            Text{.text = "Left"},
            Text{.text = "Right"})
    }
        .padding(12.f)
        .fill(FillStyle::solid(Colors::black))
        .stroke(StrokeStyle::solid(Colors::white, 1.f))
        .cornerRadius(CornerRadius{8.f})
        .cursor(Cursor::Hand)
        .focusable(true)
        .onTap([tapped = tapped] {
          if (tapped) {
            *tapped = true;
          }
        });
  }
};

struct LabeledComposite {
  std::string label;

  Element body() const { return Text{.text = label}; }
};

struct InlineLinksComposite {
  State<bool> reviewPassed;

  Element body() const {
    Theme const& theme = useEnvironment<Theme>();
    return VStack{
        .spacing = 12.f,
        .alignment = Alignment::Start,
        .children = children(
            HStack{
                .spacing = 4.f,
                .alignment = Alignment::Center,
                .children = children(
                    Text{.text = "Need copy guidance before publishing?"},
                    LinkButton{
                        .label = "Open the editorial checklist",
                        .style = LinkButton::Style{.font = Font::body()},
                    })},
            HStack{
                .spacing = 4.f,
                .alignment = Alignment::Center,
                .children = children(
                    Text{.text = *reviewPassed ? "Review already passed." : "Still waiting on approval?"},
                    LinkButton{
                        .label = *reviewPassed ? "Mark review as pending" : "Mark review as approved",
                        .style = LinkButton::Style{.font = Font::footnote()},
                    },
                    Text{.text = "or"},
                    LinkButton{
                        .label = "contact support",
                        .disabled = true,
                        .style = LinkButton::Style{.font = Font::footnote()},
                    })})};
  }
};

void collectTextLabels(SceneNode const& node, std::vector<std::string>& out) {
  if (node.kind() == SceneNodeKind::Text) {
    auto const& textNode = static_cast<TextSceneNode const&>(node);
    out.push_back(textNode.text);
  }
  for (auto const& child : node.children()) {
    collectTextLabels(*child, out);
  }
}

} // namespace
