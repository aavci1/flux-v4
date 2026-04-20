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
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/PopoverCalloutShape.hpp>
#include <Flux/UI/Views/ScaleAroundCenter.hpp>
#include <Flux/UI/Views/Spacer.hpp>
#include <Flux/UI/Views/Button.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/VStack.hpp>

#include <memory>
#include <chrono>
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
  void drawLine(Point, Point, StrokeStyle const&) override {}
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

} // namespace

namespace {

Element keyedRect(std::string key, float width, float height) {
  return Element{Rectangle{}}.key(std::move(key)).size(width, height);
}

Element demoColorBlock(float width, float height) {
  return Element{Rectangle{}}.size(width, height);
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
                  .alignment = Alignment::Stretch,
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
                          "Focused examples for VStack, HStack, ZStack, Grid, and how they compose in practice.",
                      .horizontalAlignment = HorizontalAlignment::Leading,
                      .wrapping = TextWrapping::Wrap,
                  },
                  demoVStackCard(),
                  demoHStackCard(),
                  demoZStackCard()
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
