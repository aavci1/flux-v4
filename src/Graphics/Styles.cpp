#include <Flux/Graphics/Styles.hpp>

#include <algorithm>

namespace flux {

namespace {

template <typename Gradient>
void assignGradientStops(Gradient& gradient, std::initializer_list<GradientStop> stops) {
  for (GradientStop stop : stops) {
    if (gradient.stopCount >= kMaxGradientStops) {
      break;
    }
    stop.position = std::clamp(stop.position, 0.f, 1.f);
    gradient.stops[gradient.stopCount++] = stop;
  }
  auto const stopEnd = gradient.stops.begin() + gradient.stopCount;
  std::sort(gradient.stops.begin(), stopEnd, [](GradientStop const& lhs, GradientStop const& rhs) {
    return lhs.position < rhs.position;
  });
}

template <typename Gradient>
FillStyle styleFromGradient(Gradient gradient) {
  if (gradient.stopCount == 0) {
    return FillStyle::none();
  }
  if (gradient.stopCount == 1) {
    return FillStyle::solid(gradient.stops[0].color);
  }

  FillStyle s;
  s.data = gradient;
  return s;
}

} // namespace

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

FillStyle FillStyle::linearGradient(Color from, Color to, Point start, Point end) {
  return linearGradient({GradientStop{0.f, from}, GradientStop{1.f, to}}, start, end);
}

FillStyle FillStyle::linearGradient(std::initializer_list<GradientStop> stops, Point start, Point end) {
  LinearGradient gradient{};
  gradient.start = start;
  gradient.end = end;
  assignGradientStops(gradient, stops);
  return styleFromGradient(gradient);
}

FillStyle FillStyle::radialGradient(Color inner, Color outer, Point center, float radius) {
  return radialGradient({GradientStop{0.f, inner}, GradientStop{1.f, outer}}, center, radius);
}

FillStyle FillStyle::radialGradient(std::initializer_list<GradientStop> stops, Point center, float radius) {
  RadialGradient gradient{};
  gradient.center = center;
  gradient.radius = std::max(0.f, radius);
  assignGradientStops(gradient, stops);
  return styleFromGradient(gradient);
}

FillStyle FillStyle::conicalGradient(Color from, Color to, Point center, float startAngleRadians) {
  return conicalGradient({GradientStop{0.f, from}, GradientStop{1.f, to}}, center, startAngleRadians);
}

FillStyle FillStyle::conicalGradient(std::initializer_list<GradientStop> stops, Point center,
                                     float startAngleRadians) {
  ConicalGradient gradient{};
  gradient.center = center;
  gradient.startAngleRadians = startAngleRadians;
  assignGradientStops(gradient, stops);
  return styleFromGradient(gradient);
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

bool FillStyle::linearGradient(LinearGradient* out) const {
  if (auto const* gradient = std::get_if<LinearGradient>(&data)) {
    *out = *gradient;
    return true;
  }
  return false;
}

bool FillStyle::radialGradient(RadialGradient* out) const {
  if (auto const* gradient = std::get_if<RadialGradient>(&data)) {
    *out = *gradient;
    return true;
  }
  return false;
}

bool FillStyle::conicalGradient(ConicalGradient* out) const {
  if (auto const* gradient = std::get_if<ConicalGradient>(&data)) {
    *out = *gradient;
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
