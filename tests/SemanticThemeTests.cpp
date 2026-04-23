#include <doctest/doctest.h>

#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/Scene/LineSceneNode.hpp>
#include <Flux/Scene/ModifierSceneNode.hpp>
#include <Flux/Scene/PathSceneNode.hpp>
#include <Flux/Scene/RectSceneNode.hpp>
#include <Flux/Scene/TextSceneNode.hpp>
#include <Flux/UI/ComponentKey.hpp>
#include <Flux/UI/SceneBuilder.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/Line.hpp>
#include <Flux/UI/Views/PathShape.hpp>
#include <Flux/UI/Views/SelectableTextSupport.hpp>
#include <Flux/UI/Views/Text.hpp>

#include "SceneBuilderTestSupport.hpp"

#include <memory>
#include <string_view>

namespace {

using namespace flux;

class SemanticSelectionTextSystem final : public TextSystem {
public:
  std::shared_ptr<TextLayout const> layout(AttributedString const& text, float maxWidth,
                                           TextLayoutOptions const& options) override {
    return layout(std::string_view{text.utf8}, Font{}, Colors::black, maxWidth, options);
  }

  std::shared_ptr<TextLayout const> layout(std::string_view text, Font const& font, Color const& color, float maxWidth,
                                           TextLayoutOptions const&) override {
    auto layout = std::make_shared<TextLayout>();
    float const width = maxWidth > 0.f ? std::min(maxWidth, 7.f * static_cast<float>(text.size()))
                                       : 7.f * static_cast<float>(text.size());
    layout->measuredSize = Size{std::max(width, 1.f), 14.f};
    layout->firstBaseline = 10.f;
    layout->lastBaseline = 10.f;
    layout->runs.push_back(TextLayout::PlacedRun{
        .run =
            TextRun{
                .fontSize = font.size > 0.f ? font.size : 13.f,
                .color = color,
                .ascent = 10.f,
                .descent = 4.f,
                .width = std::max(width, 1.f),
            },
        .origin = Point{0.f, 10.f},
        .utf8Begin = 0,
        .utf8End = static_cast<std::uint32_t>(text.size()),
        .ctLineIndex = 0,
    });
    layout->lines.push_back(TextLayout::LineRange{
        .ctLineIndex = 0,
        .byteStart = 0,
        .byteEnd = static_cast<int>(text.size()),
        .lineMinX = 0.f,
        .top = 0.f,
        .bottom = 14.f,
        .baseline = 10.f,
    });
    return layout;
  }

  Size measure(AttributedString const& text, float maxWidth, TextLayoutOptions const& options) override {
    return layout(text, maxWidth, options)->measuredSize;
  }

  Size measure(std::string_view text, Font const& font, Color const& color, float maxWidth,
               TextLayoutOptions const& options) override {
    return layout(text, font, color, maxWidth, options)->measuredSize;
  }

  std::uint32_t resolveFontId(std::string_view, float, bool) override { return 0; }

  std::vector<std::uint8_t> rasterizeGlyph(std::uint32_t, std::uint16_t, float, std::uint32_t&,
                                           std::uint32_t&, Point&) override {
    return {};
  }
};

template<typename NodeT>
NodeT* findNode(SceneNode* node) {
  if (!node) {
    return nullptr;
  }
  if (auto* typed = dynamic_cast<NodeT*>(node)) {
    return typed;
  }
  for (std::unique_ptr<SceneNode> const& child : node->children()) {
    if (NodeT* found = findNode<NodeT>(child.get())) {
      return found;
    }
  }
  return nullptr;
}

} // namespace

TEST_CASE("Theme: semantic color tokens resolve against light and dark themes") {
  Theme const light = Theme::light();
  Theme const dark = Theme::dark();

  CHECK(resolveColor(Color::primary(), light) == light.labelColor);
  CHECK(resolveColor(Color::secondary(), light) == light.secondaryLabelColor);
  CHECK(resolveColor(Color::tertiary(), light) == light.tertiaryLabelColor);
  CHECK(resolveColor(Color::quaternary(), light) == light.quaternaryLabelColor);
  CHECK(resolveColor(Color::placeholder(), light) == light.placeholderTextColor);
  CHECK(resolveColor(Color::disabled(), light) == light.disabledTextColor);
  CHECK(resolveColor(Color::accent(), light) == light.accentColor);
  CHECK(resolveColor(Color::accentForeground(), light) == light.accentForegroundColor);
  CHECK(resolveColor(Color::windowBackground(), light) == light.windowBackgroundColor);
  CHECK(resolveColor(Color::controlBackground(), light) == light.controlBackgroundColor);
  CHECK(resolveColor(Color::elevatedBackground(), light) == light.elevatedBackgroundColor);
  CHECK(resolveColor(Color::textBackground(), light) == light.textBackgroundColor);
  CHECK(resolveColor(Color::separator(), light) == light.separatorColor);
  CHECK(resolveColor(Color::opaqueSeparator(), light) == light.opaqueSeparatorColor);
  CHECK(resolveColor(Color::selectedContentBackground(), light) == light.selectedContentBackgroundColor);
  CHECK(resolveColor(Color::focusRing(), light) == light.keyboardFocusIndicatorColor);
  CHECK(resolveColor(Color::scrim(), light) == light.modalScrimColor);
  CHECK(resolveColor(Color::popoverScrim(), light) == light.popoverScrimColor);
  CHECK(resolveColor(Color::success(), light) == light.successColor);
  CHECK(resolveColor(Color::successForeground(), light) == light.successForegroundColor);
  CHECK(resolveColor(Color::successBackground(), light) == light.successBackgroundColor);
  CHECK(resolveColor(Color::warning(), light) == light.warningColor);
  CHECK(resolveColor(Color::warningForeground(), light) == light.warningForegroundColor);
  CHECK(resolveColor(Color::warningBackground(), light) == light.warningBackgroundColor);
  CHECK(resolveColor(Color::danger(), light) == light.dangerColor);
  CHECK(resolveColor(Color::dangerForeground(), light) == light.dangerForegroundColor);
  CHECK(resolveColor(Color::dangerBackground(), light) == light.dangerBackgroundColor);

  CHECK(resolveColor(Color::primary(), dark) == dark.labelColor);
  CHECK(resolveColor(Color::accent(), dark) == dark.accentColor);
  CHECK(resolveColor(Color::focusRing(), dark) == dark.keyboardFocusIndicatorColor);
  CHECK(resolveColor(Color::selectedContentBackground(), dark) == dark.selectedContentBackgroundColor);
  CHECK(resolveColor(Color::dangerBackground(), dark) == dark.dangerBackgroundColor);

  CHECK(resolveColor(Color::theme(), light.accentColor, light) == light.accentColor);
  CHECK(resolveColor(Color::theme(), dark.separatorColor, dark) == dark.separatorColor);
}

TEST_CASE("Theme: semantic font tokens resolve against light and dark themes") {
  Theme const light = Theme::light();
  Theme const dark = Theme::dark();

  CHECK(resolveFont(Font::largeTitle(), light) == light.largeTitleFont);
  CHECK(resolveFont(Font::title(), light) == light.titleFont);
  CHECK(resolveFont(Font::title2(), light) == light.title2Font);
  CHECK(resolveFont(Font::title3(), light) == light.title3Font);
  CHECK(resolveFont(Font::headline(), light) == light.headlineFont);
  CHECK(resolveFont(Font::subheadline(), light) == light.subheadlineFont);
  CHECK(resolveFont(Font::body(), light) == light.bodyFont);
  CHECK(resolveFont(Font::callout(), light) == light.calloutFont);
  CHECK(resolveFont(Font::footnote(), light) == light.footnoteFont);
  CHECK(resolveFont(Font::caption(), light) == light.captionFont);
  CHECK(resolveFont(Font::caption2(), light) == light.caption2Font);
  CHECK(resolveFont(Font::monospacedBody(), light) == light.monospacedBodyFont);

  CHECK(resolveFont(Font::largeTitle(), dark) == dark.largeTitleFont);
  CHECK(resolveFont(Font::monospacedBody(), dark) == dark.monospacedBodyFont);
  CHECK(resolveFont(Font::theme(), light.headlineFont, light) == light.headlineFont);
  CHECK(resolveFont(Font::theme(), dark.bodyFont, dark) == dark.bodyFont);
}

TEST_CASE("Theme: semantic accessors pick up the current environment immediately") {
  Theme const dark = Theme::dark();

  EnvironmentLayer env{};
  env.set(dark);
  EnvironmentScope envScope{std::move(env)};

  Color const background = Color::windowBackground();
  CHECK(background.semanticToken() == 10);
  CHECK(background.r == doctest::Approx(dark.windowBackgroundColor.r));
  CHECK(background.g == doctest::Approx(dark.windowBackgroundColor.g));
  CHECK(background.b == doctest::Approx(dark.windowBackgroundColor.b));
  CHECK(background.a == doctest::Approx(dark.windowBackgroundColor.a));

  Font const body = Font::body();
  CHECK(body.semanticToken() == 8);
  CHECK(body.family == dark.bodyFont.family);
  CHECK(body.size == doctest::Approx(dark.bodyFont.size));
  CHECK(body.weight == doctest::Approx(dark.bodyFont.weight));
  CHECK(body.italic == dark.bodyFont.italic);
}

TEST_CASE("SceneBuilder: semantic text and modifier paint resolve to concrete theme values") {
  SemanticSelectionTextSystem textSystem{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  SceneBuilder builder{textSystem, EnvironmentStack::current()};

  LayoutConstraints constraints{};
  constraints.maxWidth = 320.f;
  constraints.maxHeight = 120.f;

  Element element = Text{
      .text = "Semantic text",
      .font = Font::headline(),
      .color = Color::secondary(),
  }
                        .padding(12.f)
                        .fill(Color::accent())
                        .stroke(Color::separator(), 1.f)
                        .shadow(ShadowStyle{.radius = 6.f, .offset = {0.f, 2.f}, .color = Color::scrim()});

  std::unique_ptr<SceneNode> tree = builder.build(element, NodeId{1ull}, constraints);
  auto* wrapper = dynamic_cast<ModifierSceneNode*>(tree.get());
  REQUIRE(wrapper != nullptr);

  Theme const theme = Theme::light();
  Color fill{};
  REQUIRE(wrapper->fill.solidColor(&fill));
  CHECK(fill == theme.accentColor);
  CHECK(wrapper->stroke.color == theme.separatorColor);
  CHECK(wrapper->shadow.color == theme.modalScrimColor);

  auto* textNode = findNode<TextSceneNode>(tree.get());
  REQUIRE(textNode != nullptr);
  CHECK(textNode->font == theme.headlineFont);
  CHECK(textNode->color == theme.secondaryLabelColor);
}

TEST_CASE("SceneBuilder: semantic path and line paint resolve to concrete theme values") {
  SemanticSelectionTextSystem textSystem{};
  EnvironmentLayer env{};
  env.set(Theme::dark());
  EnvironmentScope envScope{std::move(env)};
  SceneBuilder builder{textSystem, EnvironmentStack::current()};

  Path path;
  path.rect(Rect{0.f, 0.f, 64.f, 24.f}, CornerRadius{8.f});

  LayoutConstraints constraints{};
  constraints.maxWidth = 320.f;
  constraints.maxHeight = 120.f;

  Element element = HStack{
      .spacing = 12.f,
      .children = children(
          PathShape{.path = std::move(path)}
              .fill(Color::warningBackground())
              .stroke(Color::warning(), 2.f)
              .shadow(ShadowStyle{.radius = 4.f, .offset = {0.f, 1.f}, .color = Color::scrim()}),
          Line{
              .from = {0.f, 0.f},
              .to = {60.f, 0.f},
              .stroke = StrokeStyle::solid(Color::danger(), 3.f),
          })
  }
                        .padding(16.f);

  std::unique_ptr<SceneNode> tree = builder.build(element, NodeId{1ull}, constraints);

  auto* pathNode = findNode<PathSceneNode>(tree.get());
  REQUIRE(pathNode != nullptr);
  auto* lineNode = findNode<LineSceneNode>(tree.get());
  REQUIRE(lineNode != nullptr);

  Theme const theme = Theme::dark();
  Color pathFill{};
  REQUIRE(pathNode->fill.solidColor(&pathFill));
  CHECK(pathFill == theme.warningBackgroundColor);
  CHECK(pathNode->stroke.color == theme.warningColor);
  CHECK(pathNode->shadow.color == theme.modalScrimColor);
  CHECK(lineNode->stroke.color == theme.dangerColor);
}

TEST_CASE("SceneBuilder: semantic selection highlights resolve to concrete theme values") {
  SemanticSelectionTextSystem textSystem{};
  EnvironmentLayer env{};
  env.set(Theme::light());
  EnvironmentScope envScope{std::move(env)};
  SceneBuilder builder{textSystem, EnvironmentStack::current()};

  LayoutConstraints constraints{};
  constraints.maxWidth = 320.f;
  constraints.maxHeight = 120.f;

  ComponentKey rootKey{};
  rootKey.push_back(LocalId::fromString("semantic-selectable"));

  Element element = Text{
      .text = "Token",
      .font = Font::body(),
      .color = Color::primary(),
      .selectionColor = Color::selectedContentBackground(),
      .selectable = true,
  };

  std::unique_ptr<SceneNode> tree = builder.build(element, NodeId{1ull}, constraints, nullptr, rootKey);
  auto state = detail::selectableTextState(rootKey);
  state->selection = detail::TextEditSelection{.caretByte = 5, .anchorByte = 0};

  tree = builder.build(element, NodeId{1ull}, constraints, std::move(tree), rootKey);
  REQUIRE(tree != nullptr);
  REQUIRE(tree->children().size() == 2);

  auto* highlight = dynamic_cast<RectSceneNode*>(tree->children()[0].get());
  REQUIRE(highlight != nullptr);
  Color fill{};
  REQUIRE(highlight->fill.solidColor(&fill));
  CHECK(fill == Theme::light().selectedContentBackgroundColor);
}
