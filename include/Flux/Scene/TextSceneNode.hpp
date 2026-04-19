#pragma once

/// \file Flux/Scene/TextSceneNode.hpp
///
/// Part of the Flux public API.

#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/Font.hpp>
#include <Flux/Graphics/TextLayoutOptions.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/Scene/SceneNode.hpp>

#include <memory>
#include <string>

namespace flux {

class TextSceneNode final : public SceneNode {
public:
  explicit TextSceneNode(NodeId id);

  std::string text{};
  Font font{};
  Color color = Color::theme();
  HorizontalAlignment horizontalAlignment = HorizontalAlignment::Leading;
  VerticalAlignment verticalAlignment = VerticalAlignment::Top;
  TextWrapping wrapping = TextWrapping::NoWrap;
  int maxLines = 0;
  float firstBaselineOffset = 0.f;
  float widthConstraint = 0.f;
  Point origin{};
  Rect allocation{};
  std::shared_ptr<TextLayout const> layout{};
  TextSystem* textSystem = nullptr;

  bool paints() const noexcept override { return true; }
  void rebuildLocalPaint() override;
  Rect computeOwnBounds() const override;
};

} // namespace flux
