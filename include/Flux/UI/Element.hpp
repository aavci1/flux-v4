#pragma once

#include <Flux/UI/BuildContext.hpp>
#include <Flux/UI/Component.hpp>
#include <Flux/UI/Detail/LeafBounds.hpp>
#include <Flux/UI/EventMap.hpp>
#include <Flux/UI/Leaves.hpp>
#include <Flux/UI/LayoutEngine.hpp>
#include <Flux/UI/StateStore.hpp>

#include <Flux/Graphics/Canvas.hpp>
#include <Flux/Scene/Nodes.hpp>
#include <Flux/Scene/SceneGraph.hpp>

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

namespace flux {

template<typename>
inline constexpr bool alwaysFalse = false;

namespace detail {

template<typename C>
float flexGrowOf(C const& v) {
  if constexpr (requires { v.flexGrow; }) {
    return v.flexGrow;
  }
  return 0.f;
}

template<typename C>
float flexShrinkOf(C const& v) {
  if constexpr (requires { v.flexShrink; }) {
    return v.flexShrink;
  }
  return 0.f;
}

template<typename C>
float minMainSizeOf(C const& v) {
  if constexpr (requires { v.minSize; }) {
    return v.minSize;
  }
  return 0.f;
}

} // namespace detail

class TextSystem;

class Element {
public:
  template<typename C>
  Element(C component);

  /// Copying clones the type-erased implementation so `std::vector<Element>` and
  /// brace-initialized child lists (which copy via `std::initializer_list`) work.
  Element(Element const& other);
  Element& operator=(Element const& other);
  Element(Element&&) noexcept = default;
  Element& operator=(Element&&) noexcept = default;

  void build(BuildContext& ctx) const;
  Size measure(BuildContext& ctx, LayoutConstraints const& constraints, TextSystem& textSystem) const;

  /// Optional flex overrides on the wrapper (from `withFlex`). When set, they replace the
  /// underlying component's flex hints for layout.
  float flexGrow() const;
  float flexShrink() const;
  float minMainSize() const;

  /// Sets flex metadata for this child in its parent stack. Overrides struct-level flex fields.
  Element withFlex(float grow, float shrink = 1.f, float minMain = 0.f) &&;

private:
  friend class LayoutEngine;

  struct Concept {
    virtual ~Concept() = default;
    virtual std::unique_ptr<Concept> clone() const = 0;
    virtual void build(BuildContext& ctx) const = 0;
    virtual Size measure(BuildContext& ctx, LayoutConstraints const& constraints,
                         TextSystem& textSystem) const = 0;
    virtual float flexGrow() const { return 0.f; }
    virtual float flexShrink() const { return 0.f; }
    virtual float minMainSize() const { return 0.f; }
  };

  template<typename C>
  struct Model;

  std::unique_ptr<Concept> impl_;
  std::optional<float> flexGrowOverride_;
  std::optional<float> flexShrinkOverride_;
  std::optional<float> minMainSizeOverride_;
};

template<typename C>
struct Element::Model : Concept {
  C value;
  explicit Model(C c) : value(std::move(c)) {}
  std::unique_ptr<Concept> clone() const override {
    if constexpr (std::is_copy_constructible_v<C>) {
      return std::make_unique<Model<C>>(value);
    } else {
      assert(false && "Non-copyable component cannot be placed in a children list");
      std::abort();
    }
  }
  void build(BuildContext& ctx) const override;
  Size measure(BuildContext& ctx, LayoutConstraints const& constraints, TextSystem& textSystem) const override;
  float flexGrow() const override { return detail::flexGrowOf(value); }
  float flexShrink() const override { return detail::flexShrinkOf(value); }
  float minMainSize() const override { return detail::minMainSizeOf(value); }
};

template<typename C>
void Element::Model<C>::build(BuildContext& ctx) const {
  if constexpr (CompositeComponent<C>) {
    ComponentKey const key = ctx.nextCompositeKey();
    StateStore* store = StateStore::current();
    if (store) {
      store->pushComponent(key);
    }
    Element child{value.body()};
    if (store) {
      store->popComponent();
    }
    ctx.beginCompositeBodySubtree(key);
    ctx.pushCompositeKeyTail(key);
    child.build(ctx);
    ctx.popCompositeKeyTail();
  } else if constexpr (RenderComponent<C>) {
    ComponentKey const stableKey = ctx.leafComponentKey();
    ctx.advanceChildSlot();
    Rect const frame = flux::detail::resolveLeafBounds(
        {}, ctx.layoutEngine().childFrame(), ctx.constraints());

    C const copy = value;
    NodeId const id = ctx.graph().addCustomRender(ctx.parentLayer(),
        CustomRenderNode{
            .frame = frame,
            .draw = [copy, frame](Canvas& canvas) { copy.render(canvas, frame); },
        });

    EventHandlers handlers{};
    handlers.stableTargetKey = stableKey;
    if constexpr (requires { value.onTap; }) {
      handlers.onTap = value.onTap;
    }
    if constexpr (requires { value.onPointerDown; }) {
      handlers.onPointerDown = value.onPointerDown;
    }
    if constexpr (requires { value.onPointerUp; }) {
      handlers.onPointerUp = value.onPointerUp;
    }
    if constexpr (requires { value.onPointerMove; }) {
      handlers.onPointerMove = value.onPointerMove;
    }
    if constexpr (requires { value.onScroll; }) {
      handlers.onScroll = value.onScroll;
    }
    if constexpr (requires { value.onKeyDown; }) {
      handlers.onKeyDown = value.onKeyDown;
    }
    if constexpr (requires { value.onKeyUp; }) {
      handlers.onKeyUp = value.onKeyUp;
    }
    if constexpr (requires { value.onTextInput; }) {
      handlers.onTextInput = value.onTextInput;
    }
    if constexpr (requires { value.cursor; }) {
      handlers.cursor = value.cursor;
    }
    if constexpr (requires { value.cursorPassthrough; }) {
      handlers.cursorPassthrough = value.cursorPassthrough;
    }
    if constexpr (requires { value.focusable; }) {
      handlers.focusable =
          value.focusable || static_cast<bool>(handlers.onKeyDown) || static_cast<bool>(handlers.onKeyUp) ||
          static_cast<bool>(handlers.onTextInput);
    } else {
      handlers.focusable =
          static_cast<bool>(handlers.onKeyDown) || static_cast<bool>(handlers.onKeyUp) ||
          static_cast<bool>(handlers.onTextInput);
    }
    if (handlers.onTap || handlers.onPointerDown || handlers.onPointerUp || handlers.onPointerMove ||
        handlers.onScroll || handlers.onKeyDown || handlers.onKeyUp || handlers.onTextInput || handlers.focusable ||
        handlers.cursor != Cursor::Inherit || handlers.cursorPassthrough) {
      ctx.eventMap().insert(id, std::move(handlers));
    }
  } else {
    static_assert(alwaysFalse<C>,
        "Missing Element::Model specialization for this component type. "
        "Add body() for a composite, or render(Canvas&, Rect) + "
        "measure(LayoutConstraints const&) for a render component.");
  }
}

template<typename C>
Size Element::Model<C>::measure(BuildContext& ctx, LayoutConstraints const& constraints,
                                TextSystem& textSystem) const {
  if constexpr (CompositeComponent<C>) {
    ComponentKey const key = ctx.nextCompositeKey();
    StateStore* store = StateStore::current();
    if (store) {
      store->pushComponent(key);
    }
    Element child{value.body()};
    if (store) {
      store->popComponent();
    }
    ctx.beginCompositeBodySubtree(key);
    ctx.pushCompositeKeyTail(key);
    Size const sz = child.measure(ctx, constraints, textSystem);
    ctx.popCompositeKeyTail();
    return sz;
  } else if constexpr (RenderComponent<C>) {
    ctx.advanceChildSlot();
    (void)textSystem;
    return value.measure(constraints);
  } else {
    static_assert(alwaysFalse<C>,
        "Missing Element::Model specialization for this component type. "
        "Add body() for a composite, or render(Canvas&, Rect) + "
        "measure(LayoutConstraints const&) for a render component.");
    return {};
  }
}

} // namespace flux

#include <Flux/UI/Layout.hpp>
#include <Flux/UI/Views/ScaleAroundCenter.hpp>

namespace flux {

template<>
struct Element::Model<Rectangle> final : Concept {
  Rectangle value;
  explicit Model(Rectangle c) : value(std::move(c)) {}
  std::unique_ptr<Concept> clone() const override {
    return std::make_unique<Model<Rectangle>>(value);
  }
  void build(BuildContext& ctx) const override;
  Size measure(BuildContext& ctx, LayoutConstraints const& constraints, TextSystem& textSystem) const override;
  float flexGrow() const override { return detail::flexGrowOf(value); }
  float flexShrink() const override { return detail::flexShrinkOf(value); }
  float minMainSize() const override { return detail::minMainSizeOf(value); }
};

template<>
struct Element::Model<LaidOutText> final : Concept {
  LaidOutText value;
  explicit Model(LaidOutText c) : value(std::move(c)) {}
  std::unique_ptr<Concept> clone() const override {
    return std::make_unique<Model<LaidOutText>>(value);
  }
  void build(BuildContext& ctx) const override;
  Size measure(BuildContext& ctx, LayoutConstraints const& constraints, TextSystem& textSystem) const override;
  float flexGrow() const override { return detail::flexGrowOf(value); }
  float flexShrink() const override { return detail::flexShrinkOf(value); }
  float minMainSize() const override { return detail::minMainSizeOf(value); }
};

template<>
struct Element::Model<Text> final : Concept {
  Text value;
  explicit Model(Text c) : value(std::move(c)) {}
  std::unique_ptr<Concept> clone() const override {
    return std::make_unique<Model<Text>>(value);
  }
  void build(BuildContext& ctx) const override;
  Size measure(BuildContext& ctx, LayoutConstraints const& constraints, TextSystem& textSystem) const override;
  float flexGrow() const override { return detail::flexGrowOf(value); }
  float flexShrink() const override { return detail::flexShrinkOf(value); }
  float minMainSize() const override { return detail::minMainSizeOf(value); }
};

template<>
struct Element::Model<views::Image> final : Concept {
  views::Image value;
  explicit Model(views::Image c) : value(std::move(c)) {}
  std::unique_ptr<Concept> clone() const override {
    return std::make_unique<Model<views::Image>>(value);
  }
  void build(BuildContext& ctx) const override;
  Size measure(BuildContext& ctx, LayoutConstraints const& constraints, TextSystem& textSystem) const override;
  float flexGrow() const override { return detail::flexGrowOf(value); }
  float flexShrink() const override { return detail::flexShrinkOf(value); }
  float minMainSize() const override { return detail::minMainSizeOf(value); }
};

template<>
struct Element::Model<PathShape> final : Concept {
  PathShape value;
  explicit Model(PathShape c) : value(std::move(c)) {}
  std::unique_ptr<Concept> clone() const override {
    return std::make_unique<Model<PathShape>>(value);
  }
  void build(BuildContext& ctx) const override;
  Size measure(BuildContext& ctx, LayoutConstraints const& constraints, TextSystem& textSystem) const override;
  float flexGrow() const override { return detail::flexGrowOf(value); }
  float flexShrink() const override { return detail::flexShrinkOf(value); }
  float minMainSize() const override { return detail::minMainSizeOf(value); }
};

template<>
struct Element::Model<Line> final : Concept {
  Line value;
  explicit Model(Line c) : value(std::move(c)) {}
  std::unique_ptr<Concept> clone() const override {
    return std::make_unique<Model<Line>>(value);
  }
  void build(BuildContext& ctx) const override;
  Size measure(BuildContext& ctx, LayoutConstraints const& constraints, TextSystem& textSystem) const override;
  float flexGrow() const override { return detail::flexGrowOf(value); }
  float flexShrink() const override { return detail::flexShrinkOf(value); }
  float minMainSize() const override { return detail::minMainSizeOf(value); }
};

template<>
struct Element::Model<VStack> final : Concept {
  VStack value;
  explicit Model(VStack c) : value(std::move(c)) {}
  std::unique_ptr<Concept> clone() const override {
    return std::make_unique<Model<VStack>>(value);
  }
  void build(BuildContext& ctx) const override;
  Size measure(BuildContext& ctx, LayoutConstraints const& constraints, TextSystem& textSystem) const override;
  float flexGrow() const override { return detail::flexGrowOf(value); }
  float flexShrink() const override { return detail::flexShrinkOf(value); }
  float minMainSize() const override { return detail::minMainSizeOf(value); }
};

template<>
struct Element::Model<HStack> final : Concept {
  HStack value;
  explicit Model(HStack c) : value(std::move(c)) {}
  std::unique_ptr<Concept> clone() const override {
    return std::make_unique<Model<HStack>>(value);
  }
  void build(BuildContext& ctx) const override;
  Size measure(BuildContext& ctx, LayoutConstraints const& constraints, TextSystem& textSystem) const override;
  float flexGrow() const override { return detail::flexGrowOf(value); }
  float flexShrink() const override { return detail::flexShrinkOf(value); }
  float minMainSize() const override { return detail::minMainSizeOf(value); }
};

template<>
struct Element::Model<ZStack> final : Concept {
  ZStack value;
  explicit Model(ZStack c) : value(std::move(c)) {}
  std::unique_ptr<Concept> clone() const override {
    return std::make_unique<Model<ZStack>>(value);
  }
  void build(BuildContext& ctx) const override;
  Size measure(BuildContext& ctx, LayoutConstraints const& constraints, TextSystem& textSystem) const override;
  float flexGrow() const override { return detail::flexGrowOf(value); }
  float flexShrink() const override { return detail::flexShrinkOf(value); }
  float minMainSize() const override { return detail::minMainSizeOf(value); }
};

template<>
struct Element::Model<ScaleAroundCenter> final : Concept {
  ScaleAroundCenter value;
  explicit Model(ScaleAroundCenter c) : value(std::move(c)) {}
  std::unique_ptr<Concept> clone() const override {
    return std::make_unique<Model<ScaleAroundCenter>>(value);
  }
  void build(BuildContext& ctx) const override;
  Size measure(BuildContext& ctx, LayoutConstraints const& constraints, TextSystem& textSystem) const override;
  float flexGrow() const override { return detail::flexGrowOf(value); }
  float flexShrink() const override { return detail::flexShrinkOf(value); }
  float minMainSize() const override { return detail::minMainSizeOf(value); }
};

template<>
struct Element::Model<Grid> final : Concept {
  Grid value;
  explicit Model(Grid c) : value(std::move(c)) {}
  std::unique_ptr<Concept> clone() const override {
    return std::make_unique<Model<Grid>>(value);
  }
  void build(BuildContext& ctx) const override;
  Size measure(BuildContext& ctx, LayoutConstraints const& constraints, TextSystem& textSystem) const override;
  float flexGrow() const override { return detail::flexGrowOf(value); }
  float flexShrink() const override { return detail::flexShrinkOf(value); }
  float minMainSize() const override { return detail::minMainSizeOf(value); }
};

template<>
struct Element::Model<Spacer> final : Concept {
  Spacer value;
  explicit Model(Spacer c) : value(std::move(c)) {}
  std::unique_ptr<Concept> clone() const override {
    return std::make_unique<Model<Spacer>>(value);
  }
  void build(BuildContext& ctx) const override;
  Size measure(BuildContext& ctx, LayoutConstraints const& constraints, TextSystem& textSystem) const override;
  float flexGrow() const override { return 1.f; }
  float flexShrink() const override { return 0.f; }
  float minMainSize() const override { return std::max(0.f, value.minLength); }
};

template<>
struct Element::Model<OffsetView> final : Concept {
  OffsetView value;
  explicit Model(OffsetView c) : value(std::move(c)) {}
  std::unique_ptr<Concept> clone() const override {
    return std::make_unique<Model<OffsetView>>(value);
  }
  void build(BuildContext& ctx) const override;
  Size measure(BuildContext& ctx, LayoutConstraints const& constraints, TextSystem& textSystem) const override;
  float flexGrow() const override { return detail::flexGrowOf(value); }
  float flexShrink() const override { return detail::flexShrinkOf(value); }
  float minMainSize() const override { return detail::minMainSizeOf(value); }
};

template<>
struct Element::Model<ScrollView> final : Concept {
  ScrollView value;
  explicit Model(ScrollView c) : value(std::move(c)) {}
  std::unique_ptr<Concept> clone() const override {
    return std::make_unique<Model<ScrollView>>(value);
  }
  void build(BuildContext& ctx) const override;
  Size measure(BuildContext& ctx, LayoutConstraints const& constraints, TextSystem& textSystem) const override;
  float flexGrow() const override { return detail::flexGrowOf(value); }
  float flexShrink() const override { return detail::flexShrinkOf(value); }
  float minMainSize() const override { return detail::minMainSizeOf(value); }
};

template<typename C>
Element::Element(C component) : impl_(std::make_unique<Model<C>>(std::move(component))) {}

inline Element::Element(Element const& other)
    : impl_(other.impl_ ? other.impl_->clone() : nullptr)
    , flexGrowOverride_(other.flexGrowOverride_)
    , flexShrinkOverride_(other.flexShrinkOverride_)
    , minMainSizeOverride_(other.minMainSizeOverride_) {}

inline Element& Element::operator=(Element const& other) {
  if (this != &other) {
    impl_ = other.impl_ ? other.impl_->clone() : nullptr;
    flexGrowOverride_ = other.flexGrowOverride_;
    flexShrinkOverride_ = other.flexShrinkOverride_;
    minMainSizeOverride_ = other.minMainSizeOverride_;
  }
  return *this;
}

inline float Element::flexGrow() const {
  return flexGrowOverride_.value_or(impl_->flexGrow());
}

inline float Element::flexShrink() const {
  return flexShrinkOverride_.value_or(impl_->flexShrink());
}

inline float Element::minMainSize() const {
  return minMainSizeOverride_.value_or(impl_->minMainSize());
}

inline Element Element::withFlex(float grow, float shrink, float minMain) && {
  flexGrowOverride_ = grow;
  flexShrinkOverride_ = shrink;
  minMainSizeOverride_ = minMain;
  return std::move(*this);
}

} // namespace flux

#include <Flux/UI/Views/PopoverCalloutShape.hpp>

namespace flux {

template<>
struct Element::Model<PopoverCalloutShape> final : Concept {
  PopoverCalloutShape value;
  explicit Model(PopoverCalloutShape c) : value(std::move(c)) {}
  std::unique_ptr<Concept> clone() const override {
    return std::make_unique<Model<PopoverCalloutShape>>(value);
  }
  void build(BuildContext& ctx) const override;
  Size measure(BuildContext& ctx, LayoutConstraints const& constraints, TextSystem& textSystem) const override;
  float flexGrow() const override { return detail::flexGrowOf(value); }
  float flexShrink() const override { return detail::flexShrinkOf(value); }
  float minMainSize() const override { return detail::minMainSizeOf(value); }
};

} // namespace flux
