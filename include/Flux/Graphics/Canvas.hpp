#pragma once

/// \file Flux/Graphics/Canvas.hpp
///
/// Part of the Flux public API.


#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/Path.hpp>
#include <Flux/Graphics/Styles.hpp>
#include <Flux/Graphics/ImageFillMode.hpp>
#include <Flux/Graphics/TextLayout.hpp>

#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

namespace flux {

class Image;
class Window;

enum class Backend : std::uint8_t { Metal };

class Canvas {
public:
  virtual ~Canvas();

  Canvas(const Canvas&) = delete;
  Canvas& operator=(const Canvas&) = delete;
  Canvas(Canvas&&) = delete;
  Canvas& operator=(Canvas&&) = delete;

  virtual Backend backend() const noexcept = 0;
  virtual unsigned int windowHandle() const = 0;

  virtual void resize(int width, int height) = 0;
  virtual void updateDpiScale(float scaleX, float scaleY) = 0;

  virtual void beginFrame() = 0;
  virtual void present() = 0;

  /// When the scene graph is unchanged, restore the last frame's draw list (Metal) instead of re-emitting ops.
  virtual void replayLastFrame() {}

  // -------------------------------------------------------------------------
  // State stack
  // -------------------------------------------------------------------------

  virtual void save() = 0;
  virtual void restore() = 0;

  // -------------------------------------------------------------------------
  // Transform
  // -------------------------------------------------------------------------

  virtual void setTransform(Mat3 const& m) = 0;
  virtual void transform(Mat3 const& m) = 0;
  virtual void translate(Point offset) = 0;
  virtual void translate(float x, float y) = 0;
  virtual void scale(float sx, float sy) = 0;
  virtual void scale(float s) = 0;
  virtual void rotate(float radians) = 0;
  virtual void rotate(float radians, Point pivot) = 0;
  virtual Mat3 currentTransform() const = 0;

  // -------------------------------------------------------------------------
  // Clip
  // -------------------------------------------------------------------------

  virtual void clipRect(Rect rect, bool antiAlias = false) = 0;

  virtual Rect clipBounds() const = 0;
  virtual bool quickReject(Rect rect) const = 0;

  // -------------------------------------------------------------------------
  // Opacity / blend
  // -------------------------------------------------------------------------

  virtual void setOpacity(float opacity) = 0;
  virtual float opacity() const = 0;
  virtual void setBlendMode(BlendMode mode) = 0;
  virtual BlendMode blendMode() const = 0;

  // -------------------------------------------------------------------------
  // Drawing
  // -------------------------------------------------------------------------

  virtual void drawRect(Rect const& rect, CornerRadius const& cornerRadius, FillStyle const& fill,
                        StrokeStyle const& stroke, ShadowStyle const& shadow = ShadowStyle::none()) = 0;
  virtual void drawLine(Point from, Point to, StrokeStyle const& stroke) = 0;
  virtual void drawPath(Path const& path, FillStyle const& fill, StrokeStyle const& stroke,
                        ShadowStyle const& shadow = ShadowStyle::none()) = 0;
  virtual void drawCircle(Point center, float radius, FillStyle const& fill, StrokeStyle const& stroke) = 0;

  /// Draw laid-out text. `origin` is the layout box top-left (`TextLayout::measuredSize`).
  virtual void drawTextLayout(TextLayout const& layout, Point origin) = 0;

  /// Draw `src` sub-rect of the image (pixel space) into `dst` (logical space). UVs are derived as src/size.
  virtual void drawImage(Image const& image, Rect const& src, Rect const& dst, CornerRadius const& corners = {},
                         float opacity = 1.f) = 0;

  /// Repeat the image across `dst` with a repeat sampler (1 logical pixel ≈ 1 texel).
  virtual void drawImageTiled(Image const& image, Rect const& dst, CornerRadius const& corners = {},
                               float opacity = 1.f) = 0;

  void drawImage(Image const& image, Rect const& dst, ImageFillMode fillMode = ImageFillMode::Cover,
                 CornerRadius const& corners = {}, float opacity = 1.f);

  /// Metal: `id<MTLDevice>` as `void*` (use with `loadImageFromFile(path, canvas.gpuDevice())`). Null if unavailable.
  virtual void* gpuDevice() const = 0;

  virtual void clear(Color color = Colors::transparent) = 0;

protected:
  Canvas() = default;
};

} // namespace flux
