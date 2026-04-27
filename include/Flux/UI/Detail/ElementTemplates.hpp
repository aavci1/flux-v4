#pragma once

#include <Flux/SceneGraph/SceneNode.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/MountContext.hpp>

#include <cassert>
#include <cstdlib>
#include <type_traits>
#include <utility>
#include <vector>

namespace flux {

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

  ElementType elementType() const noexcept override;
  std::type_index modelType() const noexcept override { return std::type_index(typeid(C)); }
  void const* rawValuePtr() const noexcept override { return &value; }

  Size measure(MeasureContext& ctx, LayoutConstraints const& constraints,
               LayoutHints const& hints, TextSystem& textSystem) const override;
  std::unique_ptr<scenegraph::SceneNode> mount(MountContext& ctx) const override;
  bool mountsWhenCollapsed() const override {
    if constexpr (requires { C::mountsWhenCollapsed; }) {
      return C::mountsWhenCollapsed;
    } else {
      return false;
    }
  }
  float flexGrow() const override { return detail::flexGrowOf(value); }
  float flexShrink() const override { return detail::flexShrinkOf(value); }
  std::optional<float> flexBasis() const override { return detail::flexBasisOf(value); }
  float minMainSize() const override { return detail::minMainSizeOf(value); }
};

template<typename C>
ElementType Element::Model<C>::elementType() const noexcept {
  if constexpr (std::is_same_v<C, Rectangle>) {
    return ElementType::Rectangle;
  } else if constexpr (std::is_same_v<C, Text>) {
    return ElementType::Text;
  } else if constexpr (std::is_same_v<C, views::Image>) {
    return ElementType::Image;
  } else if constexpr (std::is_same_v<C, PathShape>) {
    return ElementType::Path;
  } else if constexpr (std::is_same_v<C, Render>) {
    return ElementType::Render;
  } else if constexpr (std::is_same_v<C, VStack>) {
    return ElementType::VStack;
  } else if constexpr (std::is_same_v<C, HStack>) {
    return ElementType::HStack;
  } else if constexpr (std::is_same_v<C, ZStack>) {
    return ElementType::ZStack;
  } else if constexpr (std::is_same_v<C, Grid>) {
    return ElementType::Grid;
  } else if constexpr (std::is_same_v<C, OffsetView>) {
    return ElementType::OffsetView;
  } else if constexpr (std::is_same_v<C, ScrollView>) {
    return ElementType::ScrollView;
  } else if constexpr (std::is_same_v<C, ScaleAroundCenter>) {
    return ElementType::ScaleAroundCenter;
  } else if constexpr (std::is_same_v<C, Spacer>) {
    return ElementType::Spacer;
  } else if constexpr (std::is_same_v<C, PopoverCalloutShape>) {
    return ElementType::PopoverCalloutShape;
  } else {
    return ElementType::Unknown;
  }
}

template<typename C>
Size Element::Model<C>::measure(MeasureContext& ctx, LayoutConstraints const& constraints,
                                LayoutHints const& hints, TextSystem& textSystem) const {
  if constexpr (requires(C const& component, MeasureContext& measureContext,
                         LayoutConstraints const& layoutConstraints,
                         LayoutHints const& layoutHints, TextSystem& text) {
                  { component.measure(measureContext, layoutConstraints, layoutHints, text) } -> std::convertible_to<Size>;
                }) {
    return value.measure(ctx, constraints, hints, textSystem);
  } else if constexpr (BodyComponent<C>) {
    detail::HookLayoutScope const hookScope{constraints};
    Element child{value.body()};
    return child.measure(ctx, constraints, hints, textSystem);
  } else {
    static_assert(alwaysFalse<C>,
                  "Component must provide either measure(MeasureContext, LayoutConstraints, LayoutHints, "
                  "TextSystem) or body().");
    return {};
  }
}

template<typename C>
std::unique_ptr<scenegraph::SceneNode> Element::Model<C>::mount(MountContext& ctx) const {
  if constexpr (std::is_same_v<C, Rectangle>) {
    return detail::mountRectangle(value, ctx);
  } else if constexpr (std::is_same_v<C, Text>) {
    return detail::mountText(value, ctx);
  } else if constexpr (std::is_same_v<C, VStack>) {
    return detail::mountVStack(value, ctx);
  } else if constexpr (std::is_same_v<C, HStack>) {
    return detail::mountHStack(value, ctx);
  } else if constexpr (std::is_same_v<C, ZStack>) {
    return detail::mountZStack(value, ctx);
  } else if constexpr (std::is_same_v<C, Spacer>) {
    return detail::mountSpacer(value, ctx);
  } else if constexpr (requires(C const& component, MountContext& mountContext) {
                         { component.mount(mountContext) } -> std::same_as<std::unique_ptr<scenegraph::SceneNode>>;
                       }) {
    return value.mount(ctx);
  } else if constexpr (BodyComponent<C>) {
    auto childScope = std::make_shared<Reactive::Scope>();
    ctx.owner().onCleanup([childScope] {
      childScope->dispose();
    });
    MountContext childCtx{*childScope, ctx.environment(), ctx.textSystem(), ctx.measureContext(),
                          ctx.constraints(), ctx.hints(), ctx.redrawCallback()};
    return Reactive::withOwner(*childScope, [&] {
      detail::HookLayoutScope const hookScope{ctx.constraints()};
      Element child{value.body()};
      return child.mount(childCtx);
    });
  } else {
    static_assert(alwaysFalse<C>, "Component is not mountable in v5 Stage 4.");
    return nullptr;
  }
}

template<typename C>
Element::Element(C component)
    : impl_(std::make_shared<Model<C>>(std::move(component)))
    , measureId_(detail::nextElementMeasureId()) {}

template<typename T>
bool Element::is() const noexcept {
  return impl_ && impl_->modelType() == std::type_index(typeid(T));
}

template<typename T>
T const& Element::as() const {
  assert(is<T>());
  return *static_cast<T const*>(impl_->rawValuePtr());
}

template<typename... Args>
std::vector<Element> children(Args&&... args) {
  std::vector<Element> v;
  v.reserve(sizeof...(args));
  (v.emplace_back(std::forward<Args>(args)), ...);
  return v;
}

inline bool elementsStructurallyEqual(std::vector<Element> const& lhs,
                                      std::vector<Element> const& rhs) noexcept {
  if (lhs.size() != rhs.size()) {
    return false;
  }
  for (std::size_t i = 0; i < lhs.size(); ++i) {
    if (lhs[i].typeTag() != rhs[i].typeTag()) {
      return false;
    }
  }
  return true;
}

} // namespace flux
