#include <Flux/Graphics/Styles.hpp>

namespace flux {

FillStyle FillStyle::none() {
  FillStyle s;
  s.data = std::monostate{};
  return s;
}

FillStyle FillStyle::solid(Color c) {
  FillStyle s;
  s.data = c;
  return s;
}

bool FillStyle::isNone() const {
  return std::holds_alternative<std::monostate>(data);
}

bool FillStyle::solidColor(Color* out) const {
  if (auto* c = std::get_if<Color>(&data)) {
    *out = *c;
    return true;
  }
  return false;
}

StrokeStyle StrokeStyle::none() {
  StrokeStyle s;
  s.type = Type::None;
  return s;
}

StrokeStyle StrokeStyle::solid(Color c, float w) {
  StrokeStyle s;
  s.type = Type::Solid;
  s.color = c;
  s.width = w;
  return s;
}

bool StrokeStyle::isNone() const {
  return type == Type::None || width <= 0.f;
}

bool StrokeStyle::solidColor(Color* out) const {
  if (type != Type::Solid) {
    return false;
  }
  *out = color;
  return true;
}

} // namespace flux
