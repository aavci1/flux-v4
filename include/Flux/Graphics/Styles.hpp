#pragma once

/// \file Flux/Graphics/Styles.hpp
///
/// Part of the Flux public API.


#include <Flux/Core/Types.hpp>

#include <variant>

namespace flux {

enum class StrokeCap { Butt, Round, Square };
enum class StrokeJoin { Miter, Round, Bevel };

enum class FillRule { NonZero, EvenOdd };

/// Compositing / blend mode for drawing. The Metal backend maps each value to fixed-function
/// blend state where possible; modes that are not representable use the same factors as `Normal`.
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

  static FillStyle none();
  static FillStyle solid(Color c);

  bool isNone() const;

  bool solidColor(Color* out) const;
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

  static StrokeStyle none();
  static StrokeStyle solid(Color c, float w = 1.f);

  bool isNone() const;

  bool solidColor(Color* out) const;
};

} // namespace flux
