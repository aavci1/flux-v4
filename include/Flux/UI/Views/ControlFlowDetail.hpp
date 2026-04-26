#pragma once

#include <Flux/Reactive/Scope.hpp>
#include <Flux/SceneGraph/GroupNode.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/MeasureContext.hpp>
#include <Flux/UI/MountContext.hpp>
#include <Flux/UI/Detail/MountPosition.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace flux::detail {

inline float controlFiniteOrZero(float value) {
  return std::isfinite(value) ? std::max(0.f, value) : 0.f;
}

inline Size controlAssignedSize(LayoutConstraints const& constraints) {
  Size size{};
  size.width = std::isfinite(constraints.maxWidth)
      ? std::max(constraints.minWidth, constraints.maxWidth)
      : std::max(0.f, constraints.minWidth);
  size.height = std::isfinite(constraints.maxHeight)
      ? std::max(constraints.minHeight, constraints.maxHeight)
      : std::max(0.f, constraints.minHeight);
  return size;
}

inline LayoutConstraints controlFixedConstraints(Size size) {
  return LayoutConstraints{
      .maxWidth = std::max(0.f, size.width),
      .maxHeight = std::max(0.f, size.height),
      .minWidth = std::max(0.f, size.width),
      .minHeight = std::max(0.f, size.height),
  };
}

class ScopedEnvironmentSnapshot {
public:
  ScopedEnvironmentSnapshot(EnvironmentStack& stack,
                            std::vector<EnvironmentLayer> const& layers)
      : stack_(stack) {
    for (EnvironmentLayer const& layer : layers) {
      stack_.push(layer);
      ++pushed_;
    }
  }

  ScopedEnvironmentSnapshot(ScopedEnvironmentSnapshot const&) = delete;
  ScopedEnvironmentSnapshot& operator=(ScopedEnvironmentSnapshot const&) = delete;

  ~ScopedEnvironmentSnapshot() {
    while (pushed_ > 0) {
      stack_.pop();
      --pushed_;
    }
  }

private:
  EnvironmentStack& stack_;
  std::size_t pushed_ = 0;
};

inline Size controlMeasureElement(Element const& element, EnvironmentStack& environment,
                                  std::vector<EnvironmentLayer> const& environmentLayers,
                                  TextSystem& textSystem, LayoutConstraints const& constraints,
                                  LayoutHints const& hints) {
  ScopedEnvironmentSnapshot environmentScope{environment, environmentLayers};
  MeasureContext measureContext{textSystem};
  measureContext.pushConstraints(constraints, hints);
  Size measured = element.measure(measureContext, constraints, hints, textSystem);
  measureContext.popConstraints();
  return measured;
}

inline std::unique_ptr<scenegraph::SceneNode>
controlMountElement(Element const& element, Reactive::Scope& owner,
                    EnvironmentStack& environment,
                    std::vector<EnvironmentLayer> const& environmentLayers,
                    TextSystem& textSystem, LayoutConstraints const& constraints,
                    LayoutHints const& hints,
                    std::function<void()> const& requestRedraw) {
  ScopedEnvironmentSnapshot environmentScope{environment, environmentLayers};
  MeasureContext measureContext{textSystem};
  MountContext mountContext{owner, environment, textSystem, measureContext, constraints,
                            hints, requestRedraw};
  return element.mount(mountContext);
}

inline void controlLayoutVertical(scenegraph::GroupNode& group, Size frameSize, float spacing) {
  float y = 0.f;
  float width = frameSize.width;
  auto children = group.children();
  for (std::size_t i = 0; i < children.size(); ++i) {
    auto& child = children[i];
    detail::setLayoutPosition(*child, Point{0.f, y});
    Size const childSize = child->size();
    width = std::max(width, childSize.width);
    y += childSize.height;
    if (i + 1 < children.size()) {
      y += spacing;
    }
  }
  group.setSize(Size{controlFiniteOrZero(width), controlFiniteOrZero(std::max(frameSize.height, y))});
}

inline void controlLayoutSingle(scenegraph::GroupNode& group, Size frameSize) {
  Size size = frameSize;
  auto children = group.children();
  if (!children.empty()) {
    Size const childSize = children.front()->size();
    size.width = std::max(size.width, childSize.width);
    size.height = std::max(size.height, childSize.height);
  }
  group.setSize(Size{controlFiniteOrZero(size.width), controlFiniteOrZero(size.height)});
}

template<typename Factory>
Element invokeElementFactory(Factory& factory) {
  auto result = std::invoke(factory);
  return Element{std::move(result)};
}

template<typename Factory, typename T>
Element invokeForFactory(Factory& factory, T const& item,
                         Reactive::Signal<std::size_t> const& index) {
  if constexpr (std::is_invocable_v<Factory&, T const&, Reactive::Signal<std::size_t>>) {
    auto result = std::invoke(factory, item, index);
    return Element{std::move(result)};
  } else {
    static_assert(std::is_invocable_v<Factory&, T const&>,
                  "For row factory must accept (T const&) or (T const&, Signal<size_t>).");
    auto result = std::invoke(factory, item);
    return Element{std::move(result)};
  }
}

template<typename Condition>
bool readCondition(Condition& condition) {
  if constexpr (std::is_invocable_v<Condition&>) {
    return static_cast<bool>(std::invoke(condition));
  } else {
    return static_cast<bool>(condition);
  }
}

template<typename Condition>
bool readConditionCopy(Condition condition) {
  return readCondition(condition);
}

template<typename Selector>
auto readSelector(Selector& selector) {
  if constexpr (std::is_invocable_v<Selector&>) {
    return std::invoke(selector);
  } else {
    return selector;
  }
}

template<typename Selector>
auto readSelectorCopy(Selector selector) {
  return readSelector(selector);
}

} // namespace flux::detail
