#pragma once

/// \file Flux/UI/Element.hpp
///
/// Type-erased UI component wrapper: holds any view or composite, dispatches \c build / \c measure,
/// optional flex overrides, and per-subtree environment values.

#include <Flux/UI/BuildContext.hpp>
#include <Flux/UI/Component.hpp>
#include <Flux/UI/Detail/LeafBounds.hpp>
#include <Flux/UI/EventMap.hpp>
#include <Flux/UI/Leaves.hpp>
#include <Flux/UI/LayoutEngine.hpp>
#include <Flux/UI/StateStore.hpp>
#include <Flux/UI/Environment.hpp>

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

/// Erased handle to a component model (\ref CompositeComponent with \c body(), or a render leaf).
/// Copying allocates a new \c measureId_ for memoization correctness.
class Element {
public:
  /// Wraps a concrete component type; \c C must satisfy \ref CompositeComponent or \ref RenderComponent.
  template<typename C>
  Element(C component);

  /// Copying clones the type-erased implementation so `std::vector<Element>` and
  /// brace-initialized child lists (which copy via `std::initializer_list`) work.
  Element(Element const& other);
  Element& operator=(Element const& other);
  Element(Element&&) noexcept = default;
  Element& operator=(Element&&) noexcept = default;

  void build(BuildContext& ctx) const;
  Size measure(BuildContext& ctx, LayoutConstraints const& constraints, LayoutHints const& hints,
               TextSystem& textSystem) const;

  /// Optional flex overrides on the wrapper (from `withFlex`). When set, they replace the
  /// underlying component's flex hints for layout.
  float flexGrow() const;
  float flexShrink() const;
  float minMainSize() const;

  /// Sets flex metadata for this child in its parent stack. Overrides struct-level flex fields.
  Element withFlex(float grow, float shrink = 1.f, float minMain = 0.f) &&;

  /// Pushes environment values for this subtree's build and measure passes.
  template<typename T>
  Element environment(T value) && {
    if (!envLayer_) {
      envLayer_.emplace();
    }
    envLayer_->set(std::move(value));
    return std::move(*this);
  }

private:
  friend class LayoutEngine;
  friend Popover* detail::popoverOverlayStateIf(Element& el);

  struct Concept {
    virtual ~Concept() = default;
    virtual std::unique_ptr<Concept> clone() const = 0;
    virtual void build(BuildContext& ctx) const = 0;
    virtual Size measure(BuildContext& ctx, LayoutConstraints const& constraints,
                         LayoutHints const& hints, TextSystem& textSystem) const = 0;
    virtual float flexGrow() const { return 0.f; }
    virtual float flexShrink() const { return 0.f; }
    virtual float minMainSize() const { return 0.f; }
    /// When true, \ref Element::measure may return a cached \ref Size for the same
    /// `(measureId, constraints)` within a single rebuild after replaying \ref BuildContext::advanceChildSlot.
    /// Composites and layouts must return false. Memoization is safe because each `Element` has a stable
    /// \ref measureId_ and \ref MeasureCache is cleared every rebuild — the cache does not key on mutable
    /// content (e.g. string value); different instances get different ids. Leaves whose \ref measure
    /// depends on reactive state that is not part of the key should return false if that state can change
    /// between the first measure and a later replay in the same pass (see \ref MeasureCache).
    virtual bool canMemoizeMeasure() const { return false; }
  };

  template<typename C>
  struct Model;

  std::unique_ptr<Concept> impl_;
  std::optional<float> flexGrowOverride_;
  std::optional<float> flexShrinkOverride_;
  std::optional<float> minMainSizeOverride_;
  std::optional<EnvironmentLayer> envLayer_;
  /// Stable for this \ref Element instance; used by \ref MeasureCache (not `impl*` — addresses can
  /// be recycled by the allocator across short-lived temporaries).
  std::uint64_t measureId_{};
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
  Size measure(BuildContext& ctx, LayoutConstraints const& constraints, LayoutHints const& hints,
               TextSystem& textSystem) const override;
  float flexGrow() const override { return detail::flexGrowOf(value); }
  float flexShrink() const override { return detail::flexShrinkOf(value); }
  float minMainSize() const override { return detail::minMainSizeOf(value); }
  bool canMemoizeMeasure() const override {
    if constexpr (CompositeComponent<C>) {
      return false;
    } else if constexpr (RenderComponent<C>) {
      return true;
    }
    return false;
  }
};

template<typename C>
void Element::Model<C>::build(BuildContext& ctx) const {
  if constexpr (CompositeComponent<C>) {
    ComponentKey const key = ctx.nextCompositeKey();
    StateStore* store = StateStore::current();
    if (store) {
      store->pushComponent(key);
      store->pushCompositeConstraints(ctx.constraints());
    }
    Element child{value.body()};
    if (store) {
      store->popCompositeConstraints();
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
        {}, ctx.layoutEngine().consumeAssignedFrame(), ctx.constraints());

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
    // Pointer hits use the same coordinate space as `frame`. Convert to widget-local (0..size) so handlers
    // do not depend on `Runtime::buildSlotRect()`, which may not match this node after nested layout.
    if constexpr (requires { value.onPointerDown; }) {
      if (value.onPointerDown) {
        handlers.onPointerDown = [pd = value.onPointerDown, frame](Point local) {
          pd(Point{local.x - frame.x, local.y - frame.y});
        };
      }
    }
    if constexpr (requires { value.onPointerUp; }) {
      if (value.onPointerUp) {
        handlers.onPointerUp = [pu = value.onPointerUp, frame](Point local) {
          pu(Point{local.x - frame.x, local.y - frame.y});
        };
      }
    }
    if constexpr (requires { value.onPointerMove; }) {
      if (value.onPointerMove) {
        handlers.onPointerMove = [pm = value.onPointerMove, frame](Point local) {
          pm(Point{local.x - frame.x, local.y - frame.y});
        };
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
        "measure(LayoutConstraints const&, LayoutHints const&) for a render component.");
  }
}

template<typename C>
Size Element::Model<C>::measure(BuildContext& ctx, LayoutConstraints const& constraints,
                                LayoutHints const& hints, TextSystem& textSystem) const {
  if constexpr (CompositeComponent<C>) {
    ComponentKey const key = ctx.nextCompositeKey();
    StateStore* store = StateStore::current();
    if (store) {
      store->pushComponent(key);
      store->pushCompositeConstraints(constraints);
    }
    Element child{value.body()};
    if (store) {
      store->popCompositeConstraints();
      store->popComponent();
    }
    ctx.beginCompositeBodySubtree(key);
    ctx.pushCompositeKeyTail(key);
    Size const sz = child.measure(ctx, constraints, hints, textSystem);
    ctx.popCompositeKeyTail();
    return sz;
  } else if constexpr (RenderComponent<C>) {
    ctx.advanceChildSlot();
    (void)textSystem;
    return value.measure(constraints, hints);
  } else {
    static_assert(alwaysFalse<C>,
        "Missing Element::Model specialization for this component type. "
        "Add body() for a composite, or render(Canvas&, Rect) + "
        "measure(LayoutConstraints const&, LayoutHints const&) for a render component.");
    return {};
  }
}

} // namespace flux

#include <Flux/UI/Layout.hpp>
#include <Flux/UI/Views/ScaleAroundCenter.hpp>

namespace flux {

/// Generates a full Element::Model<T> specialization with standard flex delegates.
/// Extra overrides (e.g. canMemoizeMeasure) go in the variadic tail.
#define FLUX_ELEMENT_MODEL(Type, ...)                                                     \
  template<>                                                                              \
  struct Element::Model<Type> final : Concept {                                           \
    Type value;                                                                           \
    explicit Model(Type c) : value(std::move(c)) {}                                      \
    std::unique_ptr<Concept> clone() const override {                                    \
      return std::make_unique<Model<Type>>(value);                                       \
    }                                                                                     \
    void build(BuildContext& ctx) const override;                                         \
    Size measure(BuildContext& ctx, LayoutConstraints const&, LayoutHints const&, TextSystem&) const override; \
    float flexGrow() const override { return detail::flexGrowOf(value); }                \
    float flexShrink() const override { return detail::flexShrinkOf(value); }            \
    float minMainSize() const override { return detail::minMainSizeOf(value); }          \
    __VA_ARGS__                                                                           \
  }

// --- Leaf types (memoizable) ---------------------------------------------------

FLUX_ELEMENT_MODEL(Rectangle,       bool canMemoizeMeasure() const override { return true; });
FLUX_ELEMENT_MODEL(LaidOutText,     bool canMemoizeMeasure() const override { return true; });
FLUX_ELEMENT_MODEL(Text,            bool canMemoizeMeasure() const override { return true; });
FLUX_ELEMENT_MODEL(views::Image,    bool canMemoizeMeasure() const override { return true; });
FLUX_ELEMENT_MODEL(PathShape,       bool canMemoizeMeasure() const override { return true; });
FLUX_ELEMENT_MODEL(Line,            bool canMemoizeMeasure() const override { return true; });

// --- Layout containers ---------------------------------------------------------

FLUX_ELEMENT_MODEL(VStack);
FLUX_ELEMENT_MODEL(HStack);
FLUX_ELEMENT_MODEL(ZStack);
FLUX_ELEMENT_MODEL(ScaleAroundCenter);
FLUX_ELEMENT_MODEL(Grid);
FLUX_ELEMENT_MODEL(OffsetView);
FLUX_ELEMENT_MODEL(ScrollView);

// Spacer has fully custom flex behavior — not generated by the macro.
template<>
struct Element::Model<Spacer> final : Concept {
  Spacer value;
  explicit Model(Spacer c) : value(std::move(c)) {}
  std::unique_ptr<Concept> clone() const override {
    return std::make_unique<Model<Spacer>>(value);
  }
  void build(BuildContext& ctx) const override;
  Size measure(BuildContext& ctx, LayoutConstraints const&, LayoutHints const&, TextSystem&) const override;
  float flexGrow() const override { return 1.f; }
  float flexShrink() const override { return 0.f; }
  float minMainSize() const override { return std::max(0.f, value.minLength); }
  bool canMemoizeMeasure() const override { return true; }
};

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

#include <Flux/UI/Views/PopoverCalloutShape.hpp>

namespace flux {

FLUX_ELEMENT_MODEL(PopoverCalloutShape);

} // namespace flux
