#pragma once

/// \file Flux/Scene/PaintCommand.hpp
///
/// Part of the Flux public API.

#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/ImageFillMode.hpp>
#include <Flux/Graphics/Path.hpp>
#include <Flux/Graphics/Styles.hpp>
#include <Flux/Graphics/TextLayout.hpp>

#include <memory>
#include <variant>

namespace flux {

class Image;
class Renderer;

struct DrawRectPaintCommand {
  Rect rect{};
  CornerRadius cornerRadius{};
  FillStyle fill = FillStyle::none();
  StrokeStyle stroke = StrokeStyle::none();
  ShadowStyle shadow = ShadowStyle::none();
};

struct DrawTextPaintCommand {
  std::shared_ptr<TextLayout const> layout{};
  Point origin{};
};

struct DrawImagePaintCommand {
  std::shared_ptr<Image> image{};
  Rect bounds{};
  ImageFillMode fillMode = ImageFillMode::Cover;
  CornerRadius cornerRadius{};
  float opacity = 1.f;
};

struct DrawPathPaintCommand {
  Path path{};
  FillStyle fill = FillStyle::none();
  StrokeStyle stroke = StrokeStyle::none();
  ShadowStyle shadow = ShadowStyle::none();
};

struct DrawLinePaintCommand {
  Point from{};
  Point to{};
  StrokeStyle stroke = StrokeStyle::none();
};

using PaintCommand =
    std::variant<DrawRectPaintCommand, DrawTextPaintCommand, DrawImagePaintCommand, DrawPathPaintCommand,
                 DrawLinePaintCommand>;

void replayPaintCommand(PaintCommand const& cmd, Renderer& renderer);

} // namespace flux
