#pragma once

/// \file Flux/UI/Element.hpp
///
/// Type-erased UI component wrapper: holds any UI component, dispatches `measure`,
/// optional flex overrides, and per-subtree environment values.

#include <Flux/Graphics/Styles.hpp>
#include <Flux/Reactive/Bindable.hpp>
#include <Flux/UI/Component.hpp>
#include <Flux/UI/Detail/ElementModifiers.hpp>
#include <Flux/UI/Environment.hpp>
#include <Flux/UI/LayoutEngine.hpp>
#include <Flux/UI/Leaves.hpp>
#include <Flux/UI/MeasureContext.hpp>

#include <functional>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <typeindex>
#include <utility>
#include <vector>

namespace flux {

class Element;
class MountContext;
class TextSystem;
struct Popover;
struct Rectangle;
struct Text;
struct PathShape;
struct Render;
struct VStack;
struct HStack;
struct ZStack;
struct Grid;
struct OffsetView;
struct ScrollView;
struct ScaleAroundCenter;
struct Spacer;
struct PopoverCalloutShape;
namespace views {
struct Image;
} // namespace views
namespace scenegraph {
class SceneNode;
}
namespace detail {
std::uint64_t nextElementMeasureId();
} // namespace detail

template<typename>
inline constexpr bool alwaysFalse = false;

enum class ElementType : std::uint8_t {
  Unknown,
  Rectangle,
  Text,
  Image,
  Path,
  Render,
  VStack,
  HStack,
  ZStack,
  Grid,
  OffsetView,
  ScrollView,
  ScaleAroundCenter,
  Spacer,
  PopoverCalloutShape,
};

class Element {
public:
  template<typename C>
  Element(C component);

  Element(Element const& other);
  Element& operator=(Element const& other);
  Element(Element&&) noexcept = default;
  Element& operator=(Element&&) noexcept = default;

  Size measure(MeasureContext& ctx, LayoutConstraints const& constraints, LayoutHints const& hints, TextSystem& textSystem) const;
  [[nodiscard]] std::uint64_t measureId() const noexcept { return measureId_; }
  std::unique_ptr<scenegraph::SceneNode> mount(MountContext& ctx) const;
  [[nodiscard]] ElementType typeTag() const noexcept { return impl_ ? impl_->elementType() : ElementType::Unknown; }
  [[nodiscard]] detail::ElementModifiers const* modifiers() const noexcept {
    return modifiers_.get();
  }
  [[nodiscard]] EnvironmentLayer const* environmentLayer() const noexcept {
    return envLayer_ ? &*envLayer_ : nullptr;
  }

  template<typename T>
  [[nodiscard]] bool is() const noexcept;

  template<typename T>
  [[nodiscard]] T const& as() const;

  float flexGrow() const;
  float flexShrink() const;
  std::optional<float> flexBasis() const;
  float minMainSize() const;

  Element flex(float grow) &&;
  Element flex(float grow, float shrink) &&;
  Element flex(float grow, float shrink, float basis) &&;
  Element key(std::string key) &&;
  [[nodiscard]] std::optional<std::string> const& explicitKey() const noexcept { return key_; }

  template<typename T>
  Element environment(T value) && {
    if (!envLayer_) {
      envLayer_.emplace();
    }
    envLayer_->set(std::move(value));
    return std::move(*this);
  }

  Element padding(float all) &&;
  Element padding(Reactive::Bindable<EdgeInsets> insets) &&;
  Element padding(EdgeInsets insets) &&;
  Element padding(float top, float right, float bottom, float left) &&;
  Element fill(Reactive::Bindable<FillStyle> style) &&;
  Element fill(FillStyle style) &&;
  Element fill(Reactive::Bindable<Color> color) &&;
  Element fill(Color color) &&;
  Element shadow(Reactive::Bindable<ShadowStyle> style) &&;
  Element shadow(ShadowStyle style) &&;
  Element size(Reactive::Bindable<float> width, Reactive::Bindable<float> height) &&;
  Element size(float width, float height) &&;
  Element width(Reactive::Bindable<float> w) &&;
  Element width(float w) &&;
  Element height(Reactive::Bindable<float> h) &&;
  Element height(float h) &&;
  Element stroke(Reactive::Bindable<StrokeStyle> style) &&;
  Element stroke(StrokeStyle style) &&;
  Element stroke(Reactive::Bindable<Color> c, Reactive::Bindable<float> width) &&;
  Element stroke(Color c, float width) &&;
  Element cornerRadius(Reactive::Bindable<CornerRadius> radius) &&;
  Element cornerRadius(CornerRadius radius) &&;
  Element cornerRadius(Reactive::Bindable<float> radius) &&;
  Element cornerRadius(float radius) &&;
  Element opacity(Reactive::Bindable<float> opacity) &&;
  Element opacity(float opacity) &&;
  Element position(Reactive::Bindable<Vec2> p) &&;
  Element position(Vec2 p) &&;
  Element position(Reactive::Bindable<float> x, Reactive::Bindable<float> y) &&;
  Element position(float x, float y) &&;
  Element translate(Reactive::Bindable<Vec2> delta) &&;
  Element translate(Vec2 delta) &&;
  Element translate(Reactive::Bindable<float> dx, Reactive::Bindable<float> dy) &&;
  Element translate(float dx, float dy) &&;
  Element clipContent(bool clip) &&;
  Element overlay(Element over) &&;

  Element onTap(std::function<void()> handler) &&;
  Element onPointerEnter(std::function<void()> handler) &&;
  Element onPointerExit(std::function<void()> handler) &&;
  Element onFocus(std::function<void()> handler) &&;
  Element onBlur(std::function<void()> handler) &&;
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
  friend Popover* detail::popoverOverlayStateIf(Element& el);

  struct Concept {
    virtual ~Concept() = default;
    virtual std::unique_ptr<Concept> clone() const = 0;
    virtual ElementType elementType() const noexcept { return ElementType::Unknown; }
    virtual std::type_index modelType() const noexcept = 0;
    virtual void const* rawValuePtr() const noexcept = 0;
    virtual Size measure(MeasureContext& ctx, LayoutConstraints const& constraints,
                         LayoutHints const& hints, TextSystem& textSystem) const = 0;
    virtual std::unique_ptr<scenegraph::SceneNode> mount(MountContext& ctx) const = 0;
    virtual float flexGrow() const { return 0.f; }
    virtual float flexShrink() const { return 0.f; }
    virtual std::optional<float> flexBasis() const { return std::nullopt; }
    virtual float minMainSize() const { return 0.f; }
  };

  template<typename C>
  struct Model;

  std::shared_ptr<Concept> impl_;
  std::optional<float> flexGrowOverride_;
  std::optional<float> flexShrinkOverride_;
  std::optional<float> flexBasisOverride_;
  std::optional<float> minMainSizeOverride_;
  std::optional<EnvironmentLayer> envLayer_;
  std::shared_ptr<detail::ElementModifiers> modifiers_;
  std::optional<std::string> key_{};
  std::uint64_t measureId_{};

  void ensureUniqueImpl();
  detail::ElementModifiers& writableModifiers();
  Size measureWithModifiersImpl(MeasureContext& ctx, LayoutConstraints const& constraints,
                                LayoutHints const& hints, TextSystem& textSystem) const;
};

template<typename... Args>
std::vector<Element> children(Args&&... args);

bool elementsStructurallyEqual(std::vector<Element> const& lhs, std::vector<Element> const& rhs) noexcept;

} // namespace flux

#include <Flux/UI/Detail/ElementTemplates.hpp>
#include <Flux/UI/Detail/ViewModifierInlines.hpp>
