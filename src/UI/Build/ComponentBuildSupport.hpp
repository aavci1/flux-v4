#pragma once

#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/SceneGraph/GroupNode.hpp>
#include <Flux/SceneGraph/ImageNode.hpp>
#include <Flux/SceneGraph/InteractionData.hpp>
#include <Flux/SceneGraph/PathNode.hpp>
#include <Flux/SceneGraph/RectNode.hpp>
#include <Flux/SceneGraph/TextNode.hpp>
#include <Flux/UI/Detail/LeafBounds.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/SelectableTextSupport.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/TextSupport.hpp>

#include "SceneGraph/SceneBounds.hpp"

#include <algorithm>
#include <cmath>
#include <memory>

namespace flux::detail::build {

constexpr float kApproxEpsilon = 1e-4f;

inline bool nearlyEqual(float lhs, float rhs, float eps = kApproxEpsilon) {
  return std::fabs(lhs - rhs) <= eps;
}

inline bool nearlyEqual(Size lhs, Size rhs) {
  return nearlyEqual(lhs.width, rhs.width) && nearlyEqual(lhs.height, rhs.height);
}

inline bool zStackAxisStretches(std::optional<Alignment> alignment) {
  return !alignment || *alignment == Alignment::Stretch;
}

inline float resolvedAssignedSpan(float assignedSpan, bool hasAssignedSpan, float fallbackSpan) {
  if (hasAssignedSpan) {
    return std::max(0.f, assignedSpan);
  }
  if (std::isfinite(fallbackSpan) && fallbackSpan > 0.f) {
    return fallbackSpan;
  }
  return 0.f;
}

inline Size rectSize(Rect rect) {
  return Size{std::max(0.f, rect.width), std::max(0.f, rect.height)};
}

inline Rect sizeRect(Size size) {
  return Rect{0.f, 0.f, std::max(0.f, size.width), std::max(0.f, size.height)};
}

inline Rect unionRect(Rect a, Rect b) {
  if (a.width <= 0.f && a.height <= 0.f) {
    return b;
  }
  if (b.width <= 0.f && b.height <= 0.f) {
    return a;
  }
  float const minX = std::min(a.x, b.x);
  float const minY = std::min(a.y, b.y);
  float const maxX = std::max(a.x + a.width, b.x + b.width);
  float const maxY = std::max(a.y + a.height, b.y + b.height);
  return Rect {minX, minY, maxX - minX, maxY - minY};
}

inline void setGroupBounds(scenegraph::SceneNode& node, Size minSize = {}) {
  Rect bounds = sizeRect(minSize);
  for (std::unique_ptr<scenegraph::SceneNode> const& child : node.children()) {
    Rect const childBounds =
        scenegraph::detail::transformBounds(Mat3::translate(child->position()) * child->transform(),
                                            child->localBounds());
    bounds = unionRect(bounds, childBounds);
  }
  node.setBounds(bounds);
}

inline void setAssignedGroupBounds(scenegraph::SceneNode& node, Size size) {
  node.setBounds(sizeRect(size));
}

inline LocalId childLocalId(Element const& child, std::size_t index) {
  if (child.explicitKey()) {
    return LocalId::fromString(*child.explicitKey());
  }
  return LocalId::fromIndex(index);
}

inline Theme const& activeTheme(EnvironmentStack const& environment) {
  if (Theme const* theme = environment.find<Theme>()) {
    return *theme;
  }
  static Theme const fallback = Theme::light();
  return fallback;
}

inline FillStyle resolveFillStyle(FillStyle const& style, Theme const& theme) {
  FillStyle resolved = style;
  Color color{};
  if (resolved.solidColor(&color)) {
    resolved.data = resolveColor(color, theme);
  }
  return resolved;
}

inline StrokeStyle resolveStrokeStyle(StrokeStyle const& style, Theme const& theme) {
  StrokeStyle resolved = style;
  if (resolved.type == StrokeStyle::Type::Solid) {
    resolved.color = resolveColor(resolved.color, theme);
  }
  return resolved;
}

inline ShadowStyle resolveShadowStyle(ShadowStyle const& style, Theme const& theme) {
  ShadowStyle resolved = style;
  resolved.color = resolveColor(resolved.color, theme);
  return resolved;
}

inline bool updateIfChanged(Size& field, Size value) {
  if (field == value) {
    return false;
  }
  field = value;
  return true;
}

inline bool updateIfChanged(Point& field, Point value) {
  if (field == value) {
    return false;
  }
  field = value;
  return true;
}

template<typename T>
bool updateIfChanged(T& field, T const& value) {
  if (field == value) {
    return false;
  }
  field = value;
  return true;
}

inline bool updateIfChanged(Font& field, Font const& value) {
  if (field.family == value.family && nearlyEqual(field.size, value.size) &&
      nearlyEqual(field.weight, value.weight) && field.italic == value.italic) {
    return false;
  }
  field = value;
  return true;
}

inline bool updateIfChanged(Path& field, Path const& value) {
  if (field.contentHash() == value.contentHash()) {
    return false;
  }
  field = value;
  return true;
}

inline void configureTextNode(scenegraph::TextNode& textNode, Rect frameRect,
                              std::shared_ptr<TextLayout const> const& layout) {
  textNode.setBounds(Rect {0.f, 0.f, frameRect.width, frameRect.height});
  textNode.setLayout(layout);
}

inline std::unique_ptr<scenegraph::InteractionData>
makeInteractionData(ElementModifiers const* mods, ComponentKey const& key) {
  if (!mods) {
    return nullptr;
  }
  auto data = std::make_unique<scenegraph::InteractionData>();
  data->stableTargetKey = key;
  data->cursor = mods->cursor;
  data->focusable = mods->focusable || static_cast<bool>(mods->onKeyDown) ||
                    static_cast<bool>(mods->onKeyUp) || static_cast<bool>(mods->onTextInput);
  data->onPointerDown = mods->onPointerDown;
  data->onPointerUp = mods->onPointerUp;
  data->onPointerMove = mods->onPointerMove;
  data->onScroll = mods->onScroll;
  data->onKeyDown = mods->onKeyDown;
  data->onKeyUp = mods->onKeyUp;
  data->onTextInput = mods->onTextInput;
  data->onTap = mods->onTap;
  if (data->isEmpty()) {
    return nullptr;
  }
  return data;
}

inline std::unique_ptr<scenegraph::InteractionData>
makeSelectableTextInteraction(ElementModifiers const* mods, ComponentKey const& key,
                              std::shared_ptr<SelectableTextState> const& state) {
  if (!state) {
    return makeInteractionData(mods, key);
  }
  std::unique_ptr<scenegraph::InteractionData> data = makeInteractionData(mods, key);
  if (!data) {
    data = std::make_unique<scenegraph::InteractionData>();
  }
  data->stableTargetKey = key;
  data->focusable = true;
  data->cursor = Cursor::IBeam;
  data->onPointerDown = [state](Point local) { handleSelectableTextPointerDown(*state, local); };
  data->onPointerMove = [state](Point local) { handleSelectableTextPointerDrag(*state, local); };
  data->onPointerUp = [state](Point) { handleSelectableTextPointerUp(*state); };
  data->onKeyDown = [state](KeyCode keyCode, Modifiers modifiers) {
    handleSelectableTextKey(*state, keyCode, modifiers);
  };
  return data;
}

inline Rect assignedFrameForLeaf(Size measuredSize, LayoutConstraints const& constraints, Size assignedSize,
                                 bool hasAssignedWidth, bool hasAssignedHeight,
                                 ElementModifiers const* mods, LayoutHints const& hints) {
  Rect explicitBox{};
  if (mods) {
    explicitBox.width = std::max(0.f, mods->sizeWidth);
    explicitBox.height = std::max(0.f, mods->sizeHeight);
  }
  float slotWidth = 0.f;
  float slotHeight = 0.f;
  bool useAssignedWidth = false;
  bool useAssignedHeight = false;
  if (hasAssignedWidth) {
    slotWidth = std::max(0.f, assignedSize.width);
    useAssignedWidth = true;
  } else if (std::isfinite(constraints.maxWidth)) {
    slotWidth = std::max(0.f, constraints.maxWidth);
    useAssignedWidth = true;
  } else if (std::isfinite(constraints.minWidth) && constraints.minWidth > 0.f) {
    slotWidth = constraints.minWidth;
    useAssignedWidth = true;
  }
  if (hasAssignedHeight) {
    slotHeight = std::max(0.f, assignedSize.height);
    useAssignedHeight = true;
  } else if (std::isfinite(constraints.maxHeight)) {
    slotHeight = std::max(0.f, constraints.maxHeight);
    useAssignedHeight = true;
  } else if (std::isfinite(constraints.minHeight) && constraints.minHeight > 0.f) {
    slotHeight = constraints.minHeight;
    useAssignedHeight = true;
  }
  Rect const childFrame{
      0.f,
      0.f,
      useAssignedWidth ? slotWidth : measuredSize.width,
      useAssignedHeight ? slotHeight : measuredSize.height,
  };
  return resolveLeafLayoutBounds(explicitBox, childFrame, constraints, hints);
}

inline Size assignedOuterSizeForFrame(Size measuredSize, LayoutConstraints const& constraints, Size assignedSize,
                                      bool hasAssignedWidth, bool hasAssignedHeight,
                                      ElementModifiers const* mods = nullptr) {
  Size size = measuredSize;
  bool const explicitWidth = mods && mods->sizeWidth > 0.f;
  bool const explicitHeight = mods && mods->sizeHeight > 0.f;
  if (!explicitWidth && hasAssignedWidth) {
    size.width = std::max(0.f, assignedSize.width);
  } else if (!explicitWidth && std::isfinite(constraints.maxWidth) && constraints.maxWidth > 0.f) {
    size.width = std::max(0.f, constraints.maxWidth);
  } else if (!explicitWidth && constraints.minWidth > 0.f) {
    size.width = std::max(size.width, constraints.minWidth);
  }

  if (!explicitHeight && hasAssignedHeight) {
    size.height = std::max(0.f, assignedSize.height);
  } else if (!explicitHeight && std::isfinite(constraints.maxHeight) && constraints.maxHeight > 0.f) {
    size.height = std::max(0.f, constraints.maxHeight);
  } else if (!explicitHeight && constraints.minHeight > 0.f) {
    size.height = std::max(size.height, constraints.minHeight);
  }
  return size;
}

inline Size measuredOuterSizeFromContent(Size contentSize, ElementModifiers const* mods) {
  if (!mods) {
    return contentSize;
  }
  Size size{
      std::max(0.f, contentSize.width + std::max(0.f, mods->padding.left) + std::max(0.f, mods->padding.right)),
      std::max(0.f, contentSize.height + std::max(0.f, mods->padding.top) + std::max(0.f, mods->padding.bottom)),
  };
  if (mods->sizeWidth > 0.f) {
    size.width = mods->sizeWidth;
  }
  if (mods->sizeHeight > 0.f) {
    size.height = mods->sizeHeight;
  }
  return size;
}

inline bool hasOverlay(ElementModifiers const* mods) {
  return mods && mods->overlay != nullptr;
}

inline bool canDeferOuterMeasurement(ElementType typeTag, ElementModifiers const* mods,
                                     bool hasOuterModifierLayers) {
  if (hasOuterModifierLayers || hasOverlay(mods)) {
    return false;
  }
  switch (typeTag) {
  case ElementType::VStack:
  case ElementType::HStack:
  case ElementType::ZStack:
  case ElementType::Grid:
  case ElementType::OffsetView:
  case ElementType::ScrollView:
  case ElementType::ScaleAroundCenter:
  case ElementType::PopoverCalloutShape:
    return true;
  default:
    return false;
  }
}

inline bool textUsesContentBox(ElementModifiers const* mods) {
  if (!mods) {
    return false;
  }
  return !mods->padding.isZero() || !mods->fill.isNone() || !mods->stroke.isNone() ||
         !mods->shadow.isNone() || !mods->cornerRadius.isZero() || mods->opacity < 1.f - 1e-6f ||
         std::fabs(mods->translation.x) > 1e-6f || std::fabs(mods->translation.y) > 1e-6f ||
         mods->clip || std::fabs(mods->positionX) > 1e-6f || std::fabs(mods->positionY) > 1e-6f ||
         mods->sizeWidth > 0.f || mods->sizeHeight > 0.f || mods->overlay != nullptr;
}

inline bool sizeApproximatelyEqual(Size lhs, Size rhs) {
  return nearlyEqual(lhs.width, rhs.width, 0.5f) && nearlyEqual(lhs.height, rhs.height, 0.5f);
}

inline Color scrollIndicatorColorForTheme(Theme const& theme) {
  return Color{
      theme.secondaryLabelColor.r,
      theme.secondaryLabelColor.g,
      theme.secondaryLabelColor.b,
      0.55f,
  };
}

} // namespace flux::detail::build
