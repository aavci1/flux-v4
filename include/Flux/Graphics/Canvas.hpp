#pragma once

#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/Path.hpp>
#include <Flux/Graphics/Styles.hpp>

#include <cstdint>
#include <memory>
#include <span>

namespace flux {

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

  // -------------------------------------------------------------------------
  // State stack
  // -------------------------------------------------------------------------

  virtual void save() = 0;
  virtual void restore() = 0;

  virtual void setFillStyle(FillStyle const& style) = 0;
  virtual FillStyle fillStyle() const = 0;
  virtual void setStrokeStyle(StrokeStyle const& style) = 0;
  virtual StrokeStyle strokeStyle() const = 0;

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
  // Drawing (uses current fill / stroke styles)
  // -------------------------------------------------------------------------

  virtual void drawRect(Rect const& rect, CornerRadius const& cornerRadius = {}) = 0;
  virtual void drawLine(Point from, Point to) = 0;
  virtual void drawPath(Path const& path) = 0;
  virtual void drawCircle(Point center, float radius) = 0;

  virtual void clear(Color color = Colors::transparent) = 0;

protected:
  Canvas() = default;
};

} // namespace flux
