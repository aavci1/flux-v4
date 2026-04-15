#pragma once

/// \file Flux/UI/Element.hpp
///
/// Type-erased UI component wrapper: holds any view or composite, dispatches \c layout / \c measure
/// and render-phase emission, optional flex overrides, and per-subtree environment values.

#include <Flux/UI/LayoutContext.hpp>
#include <Flux/UI/LayoutTree.hpp>
#include <Flux/UI/RenderContext.hpp>
#include <Flux/UI/Component.hpp>
#include <Flux/UI/Detail/LeafBounds.hpp>
#include <Flux/UI/EventMap.hpp>
#include <Flux/UI/Leaves.hpp>
#include <Flux/UI/LayoutEngine.hpp>
#include <Flux/UI/StateStore.hpp>
#include <Flux/UI/Environment.hpp>

#include <Flux/Graphics/Styles.hpp>
#include <Flux/UI/Detail/RenderComponentEmit.hpp>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace flux {

class Element;
struct Popover;

template<typename>
inline constexpr bool alwaysFalse = false;

namespace detail {

/// Monotonic id for \ref Element::measureId_; never reused (unlike heap addresses after temp
/// \ref Element destruction — see measure memoization key).
std::uint64_t nextElementMeasureId();

Popover* popoverOverlayStateIf(Element& el);

template<typename Build>
Element& resolveCompositeBody(LayoutContext& ctx, ComponentKey const& key, Build&& build) {
  if (Element* const existing = ctx.findPinnedCompositeBody(key)) {
    return *existing;
  }
  return ctx.pinCompositeBody(key, build());
}

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
  if constexpr (requires { v.minMainSize; }) {
    return v.minMainSize;
  }
  if constexpr (requires { v.minSize; }) {
    return v.minSize;
  }
  if constexpr (requires { v.minLength; }) {
    return std::max(0.f, v.minLength);
  }
  return 0.f;
}

} // namespace detail

class TextSystem;

/// Flat modifier state applied by \ref Element during \c layout / \c measure (single decoration path).
struct ElementModifiers {
  EdgeInsets padding{};
  FillStyle fill = FillStyle::none();
  StrokeStyle stroke = StrokeStyle::none();
  ShadowStyle shadow = ShadowStyle::none();
  CornerRadius cornerRadius{};
  float opacity = 1.f;
  Vec2 translation{};
  bool clip = false;
  /// Layout-space offset within the parent's assigned cell (before post-layout transforms).
  float positionX = 0.f;
  float positionY = 0.f;
  /// \c 0 on an axis means no fixed size on that axis (same as \ref Element::size).
  float sizeWidth = 0.f;
  float sizeHeight = 0.f;
  std::unique_ptr<Element> overlay;

  std::function<void()> onTap;
  std::function<void(Point)> onPointerDown;
  std::function<void(Point)> onPointerUp;
  std::function<void(Point)> onPointerMove;
  std::function<void(Vec2)> onScroll;
  std::function<void(KeyCode, Modifiers)> onKeyDown;
  std::function<void(KeyCode, Modifiers)> onKeyUp;
  std::function<void(std::string const&)> onTextInput;
  bool focusable = false;
  Cursor cursor = Cursor::Inherit;

  bool hasInteraction() const noexcept {
    return static_cast<bool>(onTap) || static_cast<bool>(onPointerDown) || static_cast<bool>(onPointerUp) ||
           static_cast<bool>(onPointerMove) || static_cast<bool>(onScroll) || static_cast<bool>(onKeyDown) ||
           static_cast<bool>(onKeyUp) || static_cast<bool>(onTextInput) || focusable ||
           cursor != Cursor::Inherit;
  }

  bool needsModifierPass() const {
    return !padding.isZero() || !fill.isNone() || !stroke.isNone() || !shadow.isNone() ||
           !cornerRadius.isZero() || opacity < 1.f - 1e-6f || std::fabs(translation.x) > 1e-6f ||
           std::fabs(translation.y) > 1e-6f || clip || std::fabs(positionX) > 1e-6f ||
           std::fabs(positionY) > 1e-6f || hasInteraction() || sizeWidth > 0.f || sizeHeight > 0.f ||
           overlay != nullptr;
  }

  ElementModifiers() = default;
  ElementModifiers(ElementModifiers const& o);
  ElementModifiers& operator=(ElementModifiers const& o);
  ElementModifiers(ElementModifiers&&) noexcept = default;
  ElementModifiers& operator=(ElementModifiers&&) noexcept = default;
  ~ElementModifiers();
};

namespace detail {

/// Wrap one pointer callback (e.g. struct-level \c onPointerDown on a \c RenderComponent).
inline std::function<void(Point)> pointerCallbackInLocalSpace(std::function<void(Point)> cb, Rect const& frame) {
  if (!cb) {
    return {};
  }
  return [cb = std::move(cb), frame](Point local) {
    cb(Point{local.x - frame.x, local.y - frame.y});
  };
}

/// Merges \ref ElementModifiers interaction handlers into \p handlers for \ref RenderComponent leaves.
/// Modifier callbacks override struct-derived handlers when set (same rule as primitives under a modifier node).
/// Pointer positions are adjusted by \p frame (\ref detail::pointerCallbackInLocalSpace).
inline void mergeRenderLeafHandlersWithActiveModifiers(EventHandlers& h, ElementModifiers const* mods,
                                                       bool suppressLeafModifierEvents,
                                                       Rect const& frame) {
  if (!mods || suppressLeafModifierEvents) {
    return;
  }
  if (mods->onTap) {
    h.onTap = mods->onTap;
  }
  if (mods->onPointerDown) {
    h.onPointerDown = pointerCallbackInLocalSpace(mods->onPointerDown, frame);
  }
  if (mods->onPointerUp) {
    h.onPointerUp = pointerCallbackInLocalSpace(mods->onPointerUp, frame);
  }
  if (mods->onPointerMove) {
    h.onPointerMove = pointerCallbackInLocalSpace(mods->onPointerMove, frame);
  }
  if (mods->onScroll) {
    h.onScroll = mods->onScroll;
  }
  if (mods->onKeyDown) {
    h.onKeyDown = mods->onKeyDown;
  }
  if (mods->onKeyUp) {
    h.onKeyUp = mods->onKeyUp;
  }
  if (mods->onTextInput) {
    h.onTextInput = mods->onTextInput;
  }
  bool const effModFocusable =
      mods->focusable || static_cast<bool>(mods->onKeyDown) || static_cast<bool>(mods->onKeyUp) ||
      static_cast<bool>(mods->onTextInput);
  if (effModFocusable) {
    h.focusable = true;
  }
  if (mods->cursor != Cursor::Inherit) {
    h.cursor = mods->cursor;
  }
}

} // namespace detail

/// Erased handle to a component model (\ref CompositeComponent, \ref PrimitiveComponent, or \ref RenderComponent).
/// Copying allocates a new \c measureId_ for memoization correctness.
class Element {
public:
  /// Wraps a concrete component type; \c C must satisfy \ref CompositeComponent, \ref PrimitiveComponent, or
  /// \ref RenderComponent.
  template<typename C>
  Element(C component);

  /// Copying clones the type-erased implementation so `std::vector<Element>` and
  /// brace-initialized child lists (which copy via `std::initializer_list`) work.
  Element(Element const& other);
  Element& operator=(Element const& other);
  Element(Element&&) noexcept = default;
  Element& operator=(Element&&) noexcept = default;

  void layout(LayoutContext& ctx) const;
  void renderFromLayout(RenderContext& ctx, LayoutNode const& node) const;
  Size measure(LayoutContext& ctx, LayoutConstraints const& constraints, LayoutHints const& hints,
               TextSystem& textSystem) const;

  /// Optional flex overrides on the wrapper (from \c flex()). When set, they replace the
  /// underlying component's flex hints for layout.
  float flexGrow() const;
  float flexShrink() const;
  float minMainSize() const;

  /// \c Rectangle / \c PathShape leaves draw modifier fill/stroke/shadow; skip duplicate modifier chrome.
  bool leafDrawsFillStrokeShadowFromModifiers() const {
    return impl_ && impl_->leafDrawsFillStrokeShadowFromModifiers();
  }

  /// Sets flex metadata for this child in its parent stack. Overrides struct-level flex fields.
  Element flex(float grow, float shrink = 1.f, float minMain = 0.f) &&;

  /// Pushes environment values for this subtree's layout and measure passes.
  template<typename T>
  Element environment(T value) && {
    if (!envLayer_) {
      envLayer_.emplace();
    }
    envLayer_->set(std::move(value));
    return std::move(*this);
  }

  Element padding(float all) &&;
  Element padding(EdgeInsets insets) &&;
  Element padding(float top, float right, float bottom, float left) &&;
  Element fill(FillStyle style) &&;
  Element shadow(ShadowStyle style) &&;
  /// Fixed size in layout space (both axes); \c 0 leaves an axis unconstrained (fill).
  Element size(float width, float height) &&;
  Element width(float w) &&;
  Element height(float h) &&;
  Element stroke(StrokeStyle style) &&;
  Element cornerRadius(CornerRadius radius) &&;
  Element cornerRadius(float radius) &&;
  Element opacity(float opacity) &&;
  /// Shifts the element within its parent's layout cell (before rendering).
  Element position(Vec2 p) &&;
  Element position(float x, float y) &&;
  /// Post-layout layer transform (does not affect layout or sibling positioning).
  Element translate(Vec2 delta) &&;
  Element translate(float dx, float dy) &&;
  Element clipContent(bool clip) &&;
  Element overlay(Element over) &&;

  Element onTap(std::function<void()> handler) &&;
  Element onPointerDown(std::function<void(Point)> handler) &&;
  Element onPointerUp(std::function<void(Point)> handler) &&;
  Element onPointerMove(std::function<void(Point)> handler) &&;
  Element onScroll(std::function<void(Vec2)> handler) &&;
  Element onKeyDown(std::function<void(KeyCode, Modifiers)> handler) &&;
  Element onKeyUp(std::function<void(KeyCode, Modifiers)> handler) &&;
  Element onTextInput(std::function<void(std::string const&)> handler) &&;
  Element focusable(bool enabled) &&;
  Element cursor(Cursor c) &&;

private:
  friend class LayoutEngine;
  friend Popover* detail::popoverOverlayStateIf(Element& el);

  struct Concept {
    virtual ~Concept() = default;
    virtual std::unique_ptr<Concept> clone() const = 0;
    virtual void layout(LayoutContext& ctx) const = 0;
    virtual void renderFromLayout(RenderContext& ctx, LayoutNode const& node) const = 0;
    virtual Size measure(LayoutContext& ctx, LayoutConstraints const& constraints,
                         LayoutHints const& hints, TextSystem& textSystem) const = 0;
    virtual float flexGrow() const { return 0.f; }
    virtual float flexShrink() const { return 0.f; }
    virtual float minMainSize() const { return 0.f; }
    /// When true, \ref Element::measure may return a cached \ref Size for the same
    /// `(measureId, constraints)` within a single rebuild after replaying \ref LayoutContext::advanceChildSlot.
    /// Composites and layouts must return false. Memoization is safe because each `Element` has a stable
    /// \ref measureId_ and \ref MeasureCache is cleared every rebuild — the cache does not key on mutable
    /// content (e.g. string value); different instances get different ids. Leaves whose \ref measure
    /// depends on reactive state that is not part of the key should return false if that state can change
    /// between the first measure and a later replay in the same pass (see \ref MeasureCache).
    virtual bool canMemoizeMeasure() const { return false; }
    /// When true, \ref RenderLayoutTree must not paint fill/stroke/shadow on the modifier chrome rect;
    /// the leaf primitive applies those from \ref ElementModifiers (e.g. \ref Rectangle, \ref PathShape).
    virtual bool leafDrawsFillStrokeShadowFromModifiers() const { return false; }
  };

  template<typename C>
  struct Model;

  std::unique_ptr<Concept> impl_;
  std::optional<float> flexGrowOverride_;
  std::optional<float> flexShrinkOverride_;
  std::optional<float> minMainSizeOverride_;
  std::optional<EnvironmentLayer> envLayer_;
  std::optional<ElementModifiers> modifiers_;
  /// Stable for this \ref Element instance; used by \ref MeasureCache (not `impl*` — addresses can
  /// be recycled by the allocator across short-lived temporaries).
  std::uint64_t measureId_{};

  void layoutWithModifiers(LayoutContext& ctx) const;
  Size measureWithModifiersImpl(LayoutContext& ctx, LayoutConstraints const& constraints,
                                LayoutHints const& hints, TextSystem& textSystem) const;
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
  void layout(LayoutContext& ctx) const override;
  void renderFromLayout(RenderContext& ctx, LayoutNode const& node) const override;
  Size measure(LayoutContext& ctx, LayoutConstraints const& constraints, LayoutHints const& hints,
               TextSystem& textSystem) const override;
  float flexGrow() const override { return detail::flexGrowOf(value); }
  float flexShrink() const override { return detail::flexShrinkOf(value); }
  float minMainSize() const override { return detail::minMainSizeOf(value); }
  bool canMemoizeMeasure() const override {
    if constexpr (CompositeComponent<C>) {
      return false;
    } else if constexpr (PrimitiveComponent<C>) {
      if constexpr (requires { C::memoizable; }) {
        return C::memoizable;
      }
      return false;
    } else if constexpr (RenderComponent<C>) {
      return true;
    }
    return false;
  }
  bool leafDrawsFillStrokeShadowFromModifiers() const override {
    if constexpr (std::is_same_v<C, Rectangle> || std::is_same_v<C, PathShape>) {
      return true;
    }
    return false;
  }
};

template<typename C>
void Element::Model<C>::layout(LayoutContext& ctx) const {
  if constexpr (CompositeComponent<C>) {
    ComponentKey const key = ctx.nextCompositeKey();
    Element& child = detail::resolveCompositeBody(ctx, key, [&]() {
      StateStore* store = StateStore::current();
      if (store) {
        store->pushComponent(key, std::type_index(typeid(C)));
        store->pushCompositeConstraints(ctx.constraints());
      }
      Element built{value.body()};
      if (store) {
        store->popCompositeConstraints();
        store->popComponent();
      }
      return built;
    });
    ctx.beginCompositeBodySubtree(key);
    ctx.pushCompositeKeyTail(key);
    child.layout(ctx);
    ctx.popCompositeKeyTail();
  } else if constexpr (RenderComponent<C>) {
    ComponentKey const stableKey = ctx.leafComponentKey();
    ctx.advanceChildSlot();
    Rect const frame = flux::detail::resolveLeafLayoutBounds(
        {}, ctx.layoutEngine().consumeAssignedFrame(), ctx.constraints(), ctx.hints());
    LayoutNode n{};
    n.kind = LayoutNode::Kind::Leaf;
    n.frame = frame;
    n.componentKey = stableKey;
    n.element = ctx.currentElement();
    n.isCustomRenderLeaf = true;
    n.constraints = ctx.constraints();
    n.hints = ctx.hints();
    ctx.pushLayoutNode(std::move(n));
  } else if constexpr (PrimitiveComponent<C>) {
    value.layout(ctx);
  } else {
    static_assert(alwaysFalse<C>,
        "Component must satisfy CompositeComponent (body()), PrimitiveComponent (layout + measure with "
        "LayoutContext), or RenderComponent (render + measure).");
  }
}

template<typename C>
void Element::Model<C>::renderFromLayout(RenderContext& ctx, LayoutNode const& node) const {
  if constexpr (CompositeComponent<C>) {
    (void)ctx;
    (void)node;
  } else if constexpr (RenderComponent<C>) {
    Rect const frame = node.frame;
    C const copy = value;
    NodeId const id = detail::emitCustomRenderNode(ctx, frame,
        [copy, frame](Canvas& canvas) { copy.render(canvas, frame); });

    EventHandlers handlers{};
    handlers.stableTargetKey = node.componentKey;
    if constexpr (requires { value.onTap; }) {
      handlers.onTap = value.onTap;
    }
    if constexpr (requires { value.onPointerDown; }) {
      if (value.onPointerDown) {
        handlers.onPointerDown = detail::pointerCallbackInLocalSpace(value.onPointerDown, frame);
      }
    }
    if constexpr (requires { value.onPointerUp; }) {
      if (value.onPointerUp) {
        handlers.onPointerUp = detail::pointerCallbackInLocalSpace(value.onPointerUp, frame);
      }
    }
    if constexpr (requires { value.onPointerMove; }) {
      if (value.onPointerMove) {
        handlers.onPointerMove = detail::pointerCallbackInLocalSpace(value.onPointerMove, frame);
      }
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
    if constexpr (requires { value.focusable; }) {
      handlers.focusable =
          value.focusable || static_cast<bool>(handlers.onKeyDown) || static_cast<bool>(handlers.onKeyUp) ||
          static_cast<bool>(handlers.onTextInput);
    } else {
      handlers.focusable =
          static_cast<bool>(handlers.onKeyDown) || static_cast<bool>(handlers.onKeyUp) ||
          static_cast<bool>(handlers.onTextInput);
    }
    detail::mergeRenderLeafHandlersWithActiveModifiers(handlers, ctx.activeElementModifiers(),
                                                       ctx.suppressLeafModifierEvents(), frame);
    detail::registerRenderLeafEvents(ctx, id, std::move(handlers));
  } else if constexpr (PrimitiveComponent<C>) {
    value.renderFromLayout(ctx, node);
  } else {
    static_assert(alwaysFalse<C>, "Invalid component type for renderFromLayout.");
  }
}

template<typename C>
Size Element::Model<C>::measure(LayoutContext& ctx, LayoutConstraints const& constraints,
                                LayoutHints const& hints, TextSystem& textSystem) const {
  if constexpr (CompositeComponent<C>) {
    ComponentKey const key = ctx.nextCompositeKey();
    Element& child = detail::resolveCompositeBody(ctx, key, [&]() {
      StateStore* store = StateStore::current();
      if (store) {
        store->pushComponent(key, std::type_index(typeid(C)));
        store->pushCompositeConstraints(constraints);
      }
      Element built{value.body()};
      if (store) {
        store->popCompositeConstraints();
        store->popComponent();
      }
      return built;
    });
    ctx.beginCompositeBodySubtree(key);
    ctx.pushCompositeKeyTail(key);
    Size const sz = child.measure(ctx, constraints, hints, textSystem);
    ctx.popCompositeKeyTail();
    return sz;
  } else if constexpr (RenderComponent<C>) {
    ctx.advanceChildSlot();
    (void)textSystem;
    return value.measure(constraints, hints);
  } else if constexpr (PrimitiveComponent<C>) {
    return value.measure(ctx, constraints, hints, textSystem);
  } else {
    static_assert(alwaysFalse<C>,
        "Component must satisfy CompositeComponent (body()), PrimitiveComponent (layout + measure with "
        "LayoutContext), or RenderComponent (render + measure).");
    return {};
  }
}

template<typename C>
Element::Element(C component)
    : impl_(std::make_unique<Model<C>>(std::move(component)))
    , measureId_(detail::nextElementMeasureId()) {}

/// Build \c std::vector<Element> from components without \c std::initializer_list (which forces
/// copy-construction for each entry). Prefer this for \c VStack / \c HStack / \c .children lists.
template<typename... Args>
std::vector<Element> children(Args&&... args) {
  std::vector<Element> v;
  v.reserve(sizeof...(args));
  (v.emplace_back(std::forward<Args>(args)), ...);
  return v;
}

} // namespace flux

namespace flux {

// ── ViewModifiers<Derived> method implementations ───────────────────────────────
// Defined here so Element is a complete type at the point of instantiation.

template<typename Derived>
Element ViewModifiers<Derived>::padding(float all) && {
  return Element{std::move(static_cast<Derived&>(*this))}.padding(all);
}

template<typename Derived>
Element ViewModifiers<Derived>::padding(EdgeInsets insets) && {
  return Element{std::move(static_cast<Derived&>(*this))}.padding(insets);
}

template<typename Derived>
Element ViewModifiers<Derived>::padding(float top, float right, float bottom, float left) && {
  return Element{std::move(static_cast<Derived&>(*this))}.padding({top, right, bottom, left});
}

template<typename Derived>
Element ViewModifiers<Derived>::fill(FillStyle style) && {
  return Element{std::move(static_cast<Derived&>(*this))}.fill(std::move(style));
}

template<typename Derived>
Element ViewModifiers<Derived>::shadow(ShadowStyle style) && {
  return Element{std::move(static_cast<Derived&>(*this))}.shadow(std::move(style));
}

template<typename Derived>
Element ViewModifiers<Derived>::size(float width, float height) && {
  return Element{std::move(static_cast<Derived&>(*this))}.size(width, height);
}

template<typename Derived>
Element ViewModifiers<Derived>::width(float w) && {
  return Element{std::move(static_cast<Derived&>(*this))}.width(w);
}

template<typename Derived>
Element ViewModifiers<Derived>::height(float h) && {
  return Element{std::move(static_cast<Derived&>(*this))}.height(h);
}

template<typename Derived>
Element ViewModifiers<Derived>::stroke(StrokeStyle style) && {
  return Element{std::move(static_cast<Derived&>(*this))}.stroke(std::move(style));
}

template<typename Derived>
Element ViewModifiers<Derived>::cornerRadius(CornerRadius radius) && {
  return Element{std::move(static_cast<Derived&>(*this))}.cornerRadius(radius);
}

template<typename Derived>
Element ViewModifiers<Derived>::cornerRadius(float radius) && {
  return Element{std::move(static_cast<Derived&>(*this))}.cornerRadius(radius);
}

template<typename Derived>
Element ViewModifiers<Derived>::opacity(float o) && {
  return Element{std::move(static_cast<Derived&>(*this))}.opacity(o);
}

template<typename Derived>
Element ViewModifiers<Derived>::position(Vec2 p) && {
  return Element{std::move(static_cast<Derived&>(*this))}.position(p);
}

template<typename Derived>
Element ViewModifiers<Derived>::position(float x, float y) && {
  return Element{std::move(static_cast<Derived&>(*this))}.position(x, y);
}

template<typename Derived>
Element ViewModifiers<Derived>::translate(Vec2 delta) && {
  return Element{std::move(static_cast<Derived&>(*this))}.translate(delta);
}

template<typename Derived>
Element ViewModifiers<Derived>::translate(float dx, float dy) && {
  return Element{std::move(static_cast<Derived&>(*this))}.translate(dx, dy);
}

template<typename Derived>
Element ViewModifiers<Derived>::clipContent(bool clip) && {
  return Element{std::move(static_cast<Derived&>(*this))}.clipContent(clip);
}

template<typename Derived>
Element ViewModifiers<Derived>::overlay(Element over) && {
  return Element{std::move(static_cast<Derived&>(*this))}.overlay(std::move(over));
}

template<typename Derived>
Element ViewModifiers<Derived>::onTap(std::function<void()> handler) && {
  return Element{std::move(static_cast<Derived&>(*this))}.onTap(std::move(handler));
}

template<typename Derived>
Element ViewModifiers<Derived>::onPointerDown(std::function<void(Point)> handler) && {
  return Element{std::move(static_cast<Derived&>(*this))}.onPointerDown(std::move(handler));
}

template<typename Derived>
Element ViewModifiers<Derived>::onPointerUp(std::function<void(Point)> handler) && {
  return Element{std::move(static_cast<Derived&>(*this))}.onPointerUp(std::move(handler));
}

template<typename Derived>
Element ViewModifiers<Derived>::onPointerMove(std::function<void(Point)> handler) && {
  return Element{std::move(static_cast<Derived&>(*this))}.onPointerMove(std::move(handler));
}

template<typename Derived>
Element ViewModifiers<Derived>::onScroll(std::function<void(Vec2)> handler) && {
  return Element{std::move(static_cast<Derived&>(*this))}.onScroll(std::move(handler));
}

template<typename Derived>
Element ViewModifiers<Derived>::onKeyDown(std::function<void(KeyCode, Modifiers)> handler) && {
  return Element{std::move(static_cast<Derived&>(*this))}.onKeyDown(std::move(handler));
}

template<typename Derived>
Element ViewModifiers<Derived>::onKeyUp(std::function<void(KeyCode, Modifiers)> handler) && {
  return Element{std::move(static_cast<Derived&>(*this))}.onKeyUp(std::move(handler));
}

template<typename Derived>
Element ViewModifiers<Derived>::onTextInput(std::function<void(std::string const&)> handler) && {
  return Element{std::move(static_cast<Derived&>(*this))}.onTextInput(std::move(handler));
}

template<typename Derived>
Element ViewModifiers<Derived>::focusable(bool enabled) && {
  return Element{std::move(static_cast<Derived&>(*this))}.focusable(enabled);
}

template<typename Derived>
Element ViewModifiers<Derived>::cursor(Cursor c) && {
  return Element{std::move(static_cast<Derived&>(*this))}.cursor(c);
}

template<typename Derived>
Element ViewModifiers<Derived>::flex(float grow, float shrink, float minMain) && {
  return Element{std::move(static_cast<Derived&>(*this))}.flex(grow, shrink, minMain);
}

template<typename Derived>
template<typename T>
Element ViewModifiers<Derived>::environment(T value) && {
  return Element{std::move(static_cast<Derived&>(*this))}.environment(std::move(value));
}

} // namespace flux
