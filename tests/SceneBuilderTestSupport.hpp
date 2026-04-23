#pragma once

#include <doctest/doctest.h>

#include <Flux/Core/Application.hpp>
#include <Flux/Core/ComponentKey.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/Reactive/Signal.hpp>
#include <Flux/SceneGraph/GroupNode.hpp>
#include <Flux/SceneGraph/InteractionData.hpp>
#include <Flux/SceneGraph/PathNode.hpp>
#include <Flux/SceneGraph/RectNode.hpp>
#include <Flux/SceneGraph/Renderer.hpp>
#include <Flux/SceneGraph/SceneGraph.hpp>
#include <Flux/SceneGraph/SceneInteraction.hpp>
#include <Flux/SceneGraph/SceneNode.hpp>
#include <Flux/SceneGraph/SceneTraversal.hpp>
#include <Flux/SceneGraph/TextNode.hpp>
#include <Flux/UI/Environment.hpp>
#include <Flux/UI/FocusController.hpp>
#include <Flux/UI/GestureTracker.hpp>
#include <Flux/UI/LayoutEngine.hpp>
#include <Flux/UI/Overlay.hpp>
#include <Flux/UI/SceneBuilder.hpp>
#include <Flux/UI/StateStore.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/ScrollView.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/VStack.hpp>

#include <cmath>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <thread>

namespace {

using namespace flux;
using namespace flux::scenegraph;

class NullTextSystem : public TextSystem {
public:
  std::shared_ptr<TextLayout const> layout(AttributedString const& text, float maxWidth,
                                           TextLayoutOptions const& options) override {
    return layout(std::string_view{text.utf8}, Font{}, Colors::black, maxWidth, options);
  }

  std::shared_ptr<TextLayout const> layout(std::string_view text, Font const& font, Color const& color,
                                           float maxWidth, TextLayoutOptions const&) override {
    auto out = std::make_shared<TextLayout>();
    float const intrinsicWidth = std::max(1.f, 8.f * static_cast<float>(text.size()));
    float const width = maxWidth > 0.f ? std::min(maxWidth, intrinsicWidth) : intrinsicWidth;
    out->measuredSize = Size{width, 14.f};
    out->firstBaseline = 10.f;
    out->lastBaseline = 10.f;
    out->runs.push_back(TextLayout::PlacedRun{
        .run =
            TextRun{
                .fontSize = font.size > 0.f ? font.size : 13.f,
                .color = color,
                .ascent = 10.f,
                .descent = 4.f,
                .width = width,
            },
        .origin = Point{0.f, 10.f},
        .utf8Begin = 0,
        .utf8End = static_cast<std::uint32_t>(text.size()),
        .ctLineIndex = 0,
    });
    out->lines.push_back(TextLayout::LineRange{
        .ctLineIndex = 0,
        .byteStart = 0,
        .byteEnd = static_cast<int>(text.size()),
        .lineMinX = 0.f,
        .top = 0.f,
        .bottom = 14.f,
        .baseline = 10.f,
    });
    return out;
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

class VariableTextSystem final : public NullTextSystem {
public:
  std::shared_ptr<TextLayout const> layout(std::string_view text, Font const& font, Color const& color,
                                           float maxWidth, TextLayoutOptions const& options) override {
    float const intrinsicWidth = std::max(1.f, 7.f * static_cast<float>(text.size()));
    float const resolvedMaxWidth = maxWidth > 0.f ? maxWidth : intrinsicWidth;
    auto out = std::make_shared<TextLayout>();
    float const lineWidth = std::min(intrinsicWidth, resolvedMaxWidth);
    float const lines = std::max(1.f, std::ceil(intrinsicWidth / resolvedMaxWidth));
    out->measuredSize = Size{lineWidth, 14.f * lines};
    out->firstBaseline = 10.f;
    out->lastBaseline = out->measuredSize.height - 4.f;
    out->runs.push_back(TextLayout::PlacedRun{
        .run =
            TextRun{
                .fontSize = font.size > 0.f ? font.size : 13.f,
                .color = color,
                .ascent = 10.f,
                .descent = 4.f,
                .width = lineWidth,
            },
        .origin = Point{0.f, 10.f},
        .utf8Begin = 0,
        .utf8End = static_cast<std::uint32_t>(text.size()),
        .ctLineIndex = 0,
    });
    out->lines.push_back(TextLayout::LineRange{
        .ctLineIndex = 0,
        .byteStart = 0,
        .byteEnd = static_cast<int>(text.size()),
        .lineMinX = 0.f,
        .top = 0.f,
        .bottom = out->measuredSize.height,
        .baseline = 10.f,
    });
    return out;
  }
};

struct EnvironmentScope {
  explicit EnvironmentScope(EnvironmentLayer layer) {
    EnvironmentStack::current().push(std::move(layer));
  }

  ~EnvironmentScope() { EnvironmentStack::current().pop(); }
};

struct StoreScope {
  StateStore store;
  StateStore* previous = nullptr;

  StoreScope() {
    previous = StateStore::current();
    StateStore::setCurrent(&store);
  }

  ~StoreScope() { StateStore::setCurrent(previous); }
};

template<typename NodeT>
NodeT* findNode(SceneNode* node) {
  if (!node) {
    return nullptr;
  }
  if (auto* typed = dynamic_cast<NodeT*>(node)) {
    return typed;
  }
  for (std::unique_ptr<SceneNode>& child : node->children()) {
    if (NodeT* found = findNode<NodeT>(child.get())) {
      return found;
    }
  }
  return nullptr;
}

template<typename NodeT>
NodeT const* findNode(SceneNode const* node) {
  return findNode<NodeT>(const_cast<SceneNode*>(node));
}

Element keyedRect(std::string key, float width, float height) {
  return Element{Rectangle{}}.key(std::move(key)).size(width, height);
}

Element demoColorBlock(float width, float height) {
  return Element{Rectangle{}}.size(width, height);
}

struct InteractiveRectScene {
  std::unique_ptr<SceneNode> root;
  SceneNode* leaf = nullptr;
};

InteractiveRectScene makeInteractiveRectScene(std::string key, bool focusable = false,
                                              std::function<void()> onTap = {}) {
  auto root = std::make_unique<GroupNode>(Rect{0.f, 0.f, 40.f, 20.f});
  auto rect = std::make_unique<RectNode>(Rect{0.f, 0.f, 40.f, 20.f}, FillStyle::solid(Colors::black));
  SceneNode* leaf = rect.get();
  auto interaction = std::make_unique<InteractionData>();
  interaction->stableTargetKey = ComponentKey{LocalId::fromString(key)};
  interaction->focusable = focusable;
  interaction->onTap = std::move(onTap);
  rect->setInteraction(std::move(interaction));
  root->appendChild(std::move(rect));
  return InteractiveRectScene{.root = std::move(root), .leaf = leaf};
}

struct HelloRoot {
  Element body() const { return Text{.text = "Hello, World!"}; }
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
        .children = children(Text{.text = "Left"}, Text{.text = "Right"}),
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

} // namespace
