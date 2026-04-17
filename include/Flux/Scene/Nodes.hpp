#pragma once

/// \file Flux/Scene/Nodes.hpp
///
/// Part of the Flux public API.


#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/Image.hpp>
#include <Flux/Graphics/ImageFillMode.hpp>
#include <Flux/Graphics/Path.hpp>
#include <Flux/Graphics/Styles.hpp>
#include <Flux/Graphics/TextLayout.hpp>

#include <Flux/Scene/NodeId.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

namespace flux {

class Canvas;

struct LayerNode {
  NodeId id{};
  Mat3 transform = Mat3::identity();
  float opacity = 1.f;
  std::optional<Rect> clip;
  BlendMode blendMode = BlendMode::Normal;
  std::vector<NodeId> children;
};

struct RectNode {
  NodeId id{};
  Rect bounds{};
  CornerRadius cornerRadius{};
  FillStyle fill = FillStyle::none();
  StrokeStyle stroke = StrokeStyle::none();
  ShadowStyle shadow = ShadowStyle::none();
};

struct TextNode {
  NodeId id{};
  std::shared_ptr<TextLayout const> layout;
  Point origin{};
  /// When non-empty, hit-testing and layout-rect union use this box (the Text `inner` rect) instead of
  /// `layout->measuredSize`, which can exceed the laid-out cell.
  Rect allocation{};
};

struct ImageNode {
  NodeId id{};
  std::shared_ptr<Image> image;
  Rect bounds{};
  ImageFillMode fillMode = ImageFillMode::Cover;
  CornerRadius cornerRadius{};
  float opacity = 1.f;
};

struct PathNode {
  NodeId id{};
  Path path{};
  FillStyle fill = FillStyle::none();
  StrokeStyle stroke = StrokeStyle::none();
  ShadowStyle shadow = ShadowStyle::none();
};

struct LineNode {
  NodeId id{};
  Point from{};
  Point to{};
  StrokeStyle stroke = StrokeStyle::none();
};

struct CustomRenderNode {
  NodeId id{};
  Rect frame{};
  std::function<void(Canvas&)> draw;
};

using LegacySceneNode =
    std::variant<LayerNode, RectNode, TextNode, ImageNode, PathNode, LineNode, CustomRenderNode>;

} // namespace flux
