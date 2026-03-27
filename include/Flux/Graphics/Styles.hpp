#pragma once

#include <Flux/Core/Types.hpp>

#include <variant>

namespace flux {

enum class StrokeCap { Butt, Round, Square };
enum class StrokeJoin { Miter, Round, Bevel };

enum class FillRule { NonZero, EvenOdd };

enum class BlendMode {
  Normal,
  Multiply,
  Screen,
  Overlay,
  Darken,
  Lighten,
  ColorDodge,
  ColorBurn,
  HardLight,
  SoftLight,
  Difference,
  Exclusion,
  Hue,
  Saturation,
  Color,
  Luminosity,
  Clear,
  Src,
  Dst,
  SrcOver,
  DstOver,
  SrcIn,
  DstIn,
  SrcOut,
  DstOut,
  SrcAtop,
  DstAtop,
  Xor
};

/// Fill for paths and shapes (solid color or none). Matches [upstream flux FillStyle](https://github.com/aavci1/flux) conceptually.
struct FillStyle {
  std::variant<std::monostate, Color> data = std::monostate{};
  FillRule fillRule = FillRule::NonZero;

  static FillStyle none() {
    FillStyle s;
    s.data = std::monostate{};
    return s;
  }

  static FillStyle solid(Color c) {
    FillStyle s;
    s.data = c;
    return s;
  }

  bool isNone() const { return std::holds_alternative<std::monostate>(data); }

  bool solidColor(Color* out) const {
    if (auto* c = std::get_if<Color>(&data)) {
      *out = *c;
      return true;
    }
    return false;
  }
};

/// Stroke for paths, lines, and stroked rects. Matches upstream `StrokeStyle` factory pattern.
struct StrokeStyle {
  enum class Type { None, Solid };
  Type type = Type::None;
  Color color = Colors::black;
  float width = 1.f;
  StrokeCap cap = StrokeCap::Butt;
  StrokeJoin join = StrokeJoin::Miter;
  float miterLimit = 4.f;

  static StrokeStyle none() {
    StrokeStyle s;
    s.type = Type::None;
    return s;
  }

  static StrokeStyle solid(Color c, float w = 1.f) {
    StrokeStyle s;
    s.type = Type::Solid;
    s.color = c;
    s.width = w;
    return s;
  }

  bool isNone() const { return type == Type::None || width <= 0.f; }

  bool solidColor(Color* out) const {
    if (type != Type::Solid) {
      return false;
    }
    *out = color;
    return true;
  }
};

} // namespace flux
