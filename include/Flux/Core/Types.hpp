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

// -----------------------------------------------------------------------------
// Rect — axis-aligned bounds + per-corner radii (no separate RRect type)
// -----------------------------------------------------------------------------

struct Rect {
  float x = 0;
  float y = 0;
  float width = 0;
  float height = 0;
  CornerRadius corners{};

  constexpr Rect() = default;
  constexpr Rect(float x, float y, float width, float height, CornerRadius c = {})
      : x(x), y(y), width(width), height(height), corners(c) {}

  static Rect rounded(float rx, float ry, float rw, float rh, float radius) {
    const float r = std::max(0.f, radius);
    return Rect{rx, ry, rw, rh, CornerRadius(r, r, r, r)};
  }

  static Rect pill(float rx, float ry, float rw, float rh) {
    const float r = std::min(rw, rh) * 0.5f;
    return Rect{rx, ry, rw, rh, CornerRadius(r, r, r, r)};
  }

  static constexpr Rect sharp(float rx, float ry, float rw, float rh) { return Rect{rx, ry, rw, rh, {}}; }

  constexpr Point center() const { return {x + width * 0.5f, y + height * 0.5f}; }

  bool intersects(Rect o) const {
    return x < o.x + o.width && x + width > o.x && y < o.y + o.height && y + height > o.y;
  }

  constexpr bool operator==(const Rect& o) const = default;
};

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
