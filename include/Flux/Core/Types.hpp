#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace flux {

// -----------------------------------------------------------------------------
// Point / Vec2 (2D position)
// -----------------------------------------------------------------------------

struct Point {
  float x = 0;
  float y = 0;

  constexpr Point() = default;
  constexpr Point(float x, float y) : x(x), y(y) {}

  constexpr Point operator+(const Point& o) const { return {x + o.x, y + o.y}; }
  constexpr Point operator-(const Point& o) const { return {x - o.x, y - o.y}; }
  constexpr Point operator*(float s) const { return {x * s, y * s}; }
  constexpr Point operator/(float s) const { return {x / s, y / s}; }

  constexpr bool operator==(const Point& o) const = default;
};

using Vec2 = Point;

// -----------------------------------------------------------------------------
// Size
// -----------------------------------------------------------------------------

struct Size {
  float width = 0;
  float height = 0;

  constexpr Size() = default;
  constexpr Size(float width, float height) : width(width), height(height) {}

  constexpr bool isEmpty() const { return width <= 0 || height <= 0; }
  constexpr float area() const { return width * height; }
  constexpr bool operator==(const Size& other) const = default;
};

// -----------------------------------------------------------------------------
// Corner radii (top-left, top-right, bottom-right, bottom-left — matches SDF shader)
// -----------------------------------------------------------------------------

struct Rect;

struct CornerRadius {
  float topLeft = 0;
  float topRight = 0;
  float bottomRight = 0;
  float bottomLeft = 0;

  constexpr CornerRadius() = default;
  explicit constexpr CornerRadius(float all) : topLeft(all), topRight(all), bottomRight(all), bottomLeft(all) {}
  constexpr CornerRadius(float tl, float tr, float br, float bl)
      : topLeft(tl), topRight(tr), bottomRight(br), bottomLeft(bl) {}

  CornerRadius& operator=(float value) {
    topLeft = topRight = bottomRight = bottomLeft = value;
    return *this;
  }

  constexpr bool isUniform() const {
    return topLeft == topRight && topRight == bottomRight && bottomRight == bottomLeft;
  }
  constexpr bool isZero() const {
    return topLeft == 0 && topRight == 0 && bottomRight == 0 && bottomLeft == 0;
  }
  constexpr bool operator==(const CornerRadius& o) const = default;

  /// Full pill / circle corner radii for an axis-aligned `bounds` (all corners = min(w,h)/2).
  static constexpr CornerRadius pill(Rect const& bounds);
};

// -----------------------------------------------------------------------------
// Color (linear RGB)
// -----------------------------------------------------------------------------

struct Color {
  float r = 0;
  float g = 0;
  float b = 0;
  float a = 1;

  constexpr Color() = default;
  constexpr Color(float r, float g, float b, float a = 1.f) : r(r), g(g), b(b), a(a) {}

  static constexpr Color rgb(std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    const float s = 1.f / 255.f;
    return Color(r * s, g * s, b * s, 1.f);
  }

  static constexpr Color hex(std::uint32_t h) {
    return rgb(static_cast<std::uint8_t>((h >> 16) & 0xFF), static_cast<std::uint8_t>((h >> 8) & 0xFF),
               static_cast<std::uint8_t>(h & 0xFF));
  }

  constexpr bool operator==(const Color& o) const = default;
};

/// Sentinel: inherit from `FluxTheme` (see `resolveColor`).
inline constexpr Color kFromTheme{0.f, 0.f, 0.f, -1.f};

constexpr inline Color resolveColor(Color override, Color themeValue) {
  return (override.a < 0.f) ? themeValue : override;
}

namespace Colors {
constexpr Color white{1, 1, 1, 1};
constexpr Color black{0, 0, 0, 1};
constexpr Color transparent{0, 0, 0, 0};
constexpr Color red = Color::hex(0xF44336);
constexpr Color blue = Color::hex(0x2196F3);
constexpr Color green = Color::hex(0x4CAF50);
constexpr Color yellow = Color::hex(0xFFD700);
constexpr Color gray = Color::hex(0x9E9E9E);
constexpr Color darkGray = Color::hex(0x424242);
constexpr Color lightGray = Color::hex(0xE0E0E0);
} // namespace Colors

/// How the current keyboard focus was last moved (pointer click vs Tab / programmatic).
enum class FocusInputKind : std::uint8_t { Pointer, Keyboard };

// -----------------------------------------------------------------------------
// Rect — axis-aligned bounds only (rounded corners use `CornerRadius` separately)
// -----------------------------------------------------------------------------

struct Rect {
  float x = 0;
  float y = 0;
  float width = 0;
  float height = 0;

  constexpr Rect() = default;
  constexpr Rect(float x, float y, float width, float height) : x(x), y(y), width(width), height(height) {}

  /// Plain axis-aligned rect (same as value constructor; name matches prior `Rect::sharp` usage).
  static constexpr Rect sharp(float rx, float ry, float rw, float rh) { return Rect{rx, ry, rw, rh}; }

  constexpr Point center() const { return {x + width * 0.5f, y + height * 0.5f}; }

  bool intersects(Rect o) const {
    return x < o.x + o.width && x + width > o.x && y < o.y + o.height && y + height > o.y;
  }

  constexpr bool contains(Point p) const {
    const float x0 = std::min(x, x + width);
    const float x1 = std::max(x, x + width);
    const float y0 = std::min(y, y + height);
    const float y1 = std::max(y, y + height);
    return p.x >= x0 && p.x <= x1 && p.y >= y0 && p.y <= y1;
  }

  constexpr bool operator==(const Rect& o) const = default;
};

constexpr CornerRadius CornerRadius::pill(Rect const& bounds) {
  const float r = std::min(bounds.width, bounds.height) * 0.5f;
  return CornerRadius(r, r, r, r);
}

// -----------------------------------------------------------------------------
// Mat3 — 3×3 affine (column-major; column-vector multiply in `apply`)
// Column 0: m[0], m[1], m[2]; column 1: m[3], m[4], m[5]; column 2 (translation): m[6], m[7], m[8].
// -----------------------------------------------------------------------------

struct Mat3 {
  float m[9]{};

  static constexpr Mat3 identity() {
    Mat3 r{};
    r.m[0] = 1.f;
    r.m[4] = 1.f;
    r.m[8] = 1.f;
    return r;
  }

  static constexpr Mat3 translate(Point offset) { return translate(offset.x, offset.y); }

  static constexpr Mat3 translate(float tx, float ty) {
    Mat3 r = identity();
    r.m[6] = tx;
    r.m[7] = ty;
    return r;
  }

  static constexpr Mat3 scale(float sx, float sy) {
    Mat3 r{};
    r.m[0] = sx;
    r.m[4] = sy;
    r.m[8] = 1.f;
    return r;
  }

  static constexpr Mat3 scale(float s) { return scale(s, s); }

  static Mat3 rotate(float radians) {
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    Mat3 r{};
    r.m[0] = c;
    r.m[1] = s;
    r.m[3] = -s;
    r.m[4] = c;
    r.m[8] = 1.f;
    return r;
  }

  static Mat3 rotate(float radians, Point pivot) {
    return translate(pivot) * rotate(radians) * translate(Point{-pivot.x, -pivot.y});
  }

  Mat3 operator*(Mat3 const& o) const {
    Mat3 r{};
    for (int j = 0; j < 3; ++j) {
      for (int i = 0; i < 3; ++i) {
        float s = 0.f;
        for (int k = 0; k < 3; ++k) {
          s += m[k * 3 + i] * o.m[j * 3 + k];
        }
        r.m[j * 3 + i] = s;
      }
    }
    return r;
  }

  Point apply(Point p) const {
    const float x = m[0] * p.x + m[3] * p.y + m[6];
    const float y = m[1] * p.x + m[4] * p.y + m[7];
    return {x, y};
  }

  /// Full 3×3 inverse. Returns `identity()` if the matrix is singular (|det| below epsilon).
  Mat3 inverse() const {
    constexpr float kEps = 1e-12f;
    const float a = m[0];
    const float b = m[1];
    const float c = m[2];
    const float d = m[3];
    const float e = m[4];
    const float f = m[5];
    const float g = m[6];
    const float h = m[7];
    const float i = m[8];
    const float det =
        a * (e * i - f * h) - d * (b * i - c * h) + g * (b * f - c * e);
    if (std::abs(det) < kEps) {
      return identity();
    }
    const float invDet = 1.f / det;
    Mat3 r{};
    r.m[0] = (e * i - f * h) * invDet;
    r.m[1] = (c * h - b * i) * invDet;
    r.m[2] = (b * f - c * e) * invDet;
    r.m[3] = (f * g - d * i) * invDet;
    r.m[4] = (a * i - c * g) * invDet;
    r.m[5] = (c * d - a * f) * invDet;
    r.m[6] = (d * h - e * g) * invDet;
    r.m[7] = (b * g - a * h) * invDet;
    r.m[8] = (a * e - b * d) * invDet;
    return r;
  }

  /// Determinant of the upper-left 2×2 (scale/rotation/shear); zero when the affine map collapses 2D area.
  float affineDeterminant() const { return m[0] * m[4] - m[1] * m[3]; }
};

enum class MouseButton : std::uint8_t { None, Left, Right, Middle, Other };

using KeyCode = std::uint16_t;

enum class Modifiers : std::uint32_t {
  None = 0,
  Shift = 1 << 0,
  Ctrl = 1 << 1,
  Alt = 1 << 2,
  Meta = 1 << 3,
};

constexpr Modifiers operator|(Modifiers a, Modifiers b) {
  return static_cast<Modifiers>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
}

constexpr Modifiers operator&(Modifiers a, Modifiers b) {
  return static_cast<Modifiers>(static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(b));
}

constexpr bool any(Modifiers m) { return static_cast<std::uint32_t>(m) != 0; }

} // namespace flux
