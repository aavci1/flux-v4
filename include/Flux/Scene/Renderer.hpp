#pragma once

/// \file Flux/Scene/Renderer.hpp
///
/// Part of the Flux public API.

#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/ImageFillMode.hpp>
#include <Flux/Graphics/Path.hpp>
#include <Flux/Graphics/Styles.hpp>
#include <Flux/Graphics/TextLayout.hpp>

#include <memory>

namespace flux {

class Image;
class Canvas;

class Renderer {
public:
  virtual ~Renderer() = default;

  virtual void save() = 0;
  virtual void restore() = 0;

  virtual void translate(Point offset) = 0;
  virtual void transform(Mat3 const& matrix) = 0;

  virtual void clipRect(Rect rect, CornerRadius const& cornerRadius = CornerRadius{}, bool antiAlias = false) = 0;
  virtual bool quickReject(Rect rect) const = 0;

  virtual void setOpacity(float opacity) = 0;
  virtual void setBlendMode(BlendMode mode) = 0;

  virtual void drawRect(Rect const& rect, CornerRadius const& cornerRadius, FillStyle const& fill,
                        StrokeStyle const& stroke, ShadowStyle const& shadow) = 0;
  virtual void drawLine(Point from, Point to, StrokeStyle const& stroke) = 0;
  virtual void drawPath(Path const& path, FillStyle const& fill, StrokeStyle const& stroke,
                        ShadowStyle const& shadow) = 0;
  virtual void drawTextLayout(TextLayout const& layout, Point origin) = 0;
  virtual void drawImage(Image const& image, Rect const& bounds, ImageFillMode fillMode,
                         CornerRadius const& cornerRadius, float opacity) = 0;

  virtual Canvas* canvas() noexcept { return nullptr; }
};

} // namespace flux
