#pragma once

#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/Canvas.hpp>
#include <Flux/Graphics/Path.hpp>
#include <Flux/Graphics/Styles.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>

using namespace flux;

namespace clock_demo {

inline void drawHand(Canvas& c, Point center, float length, float width, float angleDeg,
                     Color color) {
  c.save();
  const float rad = (180.f - angleDeg) * (std::numbers::pi_v<float> / 180.f);
  const float x = center.x + length * std::sin(rad);
  const float y = center.y + length * std::cos(rad);
  c.drawLine(center, {x, y}, StrokeStyle::solid(color, width));
  c.restore();
}

/// Hand angles in degrees (same convention as the original `hour24`/`minutes`/`seconds` mapping:
/// hour `hours12 * 30 + minutes * 0.5`, minute `minutes * 6 + seconds * 0.1`, second `seconds * 6`).
inline void drawClock(Canvas& c, Rect const& bounds, float hourAngleDeg, float minuteAngleDeg,
                      float secondAngleDeg) {
  const float radius = std::min(bounds.width, bounds.height) * 0.5f - 20.f;
  const Point center = bounds.center();

  c.drawCircle(center, radius, FillStyle::solid(Colors::white), StrokeStyle::solid(Colors::black, 20.f));

  const float hourTickLength = radius / 10.f;
  const float hourTickWidth = 6.f;
  Path hourMarks;
  for (int i = 0; i < 12; ++i) {
    const float angle = static_cast<float>(i) * 2.f * std::numbers::pi_v<float> / 12.f;
    const float x1 = center.x + (radius - 20.f - hourTickLength) * std::cos(angle);
    const float y1 = center.y + (radius - 20.f - hourTickLength) * std::sin(angle);
    const float x2 = center.x + (radius - 20.f) * std::cos(angle);
    const float y2 = center.y + (radius - 20.f) * std::sin(angle);
    hourMarks.moveTo({x1, y1});
    hourMarks.lineTo({x2, y2});
  }
  c.drawPath(hourMarks, FillStyle::none(), StrokeStyle::solid(Colors::black, hourTickWidth));

  const float minuteTickLength = radius / 10.f;
  const float minuteTickWidth = 2.f;
  Path minuteMarks;
  for (int i = 0; i < 60; ++i) {
    const float angle = static_cast<float>(i) * 2.f * std::numbers::pi_v<float> / 60.f;
    const float x1 = center.x + (radius - 20.f - minuteTickLength) * std::cos(angle);
    const float y1 = center.y + (radius - 20.f - minuteTickLength) * std::sin(angle);
    const float x2 = center.x + (radius - 20.f) * std::cos(angle);
    const float y2 = center.y + (radius - 20.f) * std::sin(angle);
    minuteMarks.moveTo({x1, y1});
    minuteMarks.lineTo({x2, y2});
  }
  c.drawPath(minuteMarks, FillStyle::none(), StrokeStyle::solid(Colors::black, minuteTickWidth));

  drawHand(c, center, radius * 0.4f, 12.f, hourAngleDeg, Colors::black);
  drawHand(c, center, radius * 0.55f, 8.f, minuteAngleDeg, Colors::black);
  drawHand(c, center, radius * 0.7f, 4.f, secondAngleDeg, Colors::red);

  c.drawCircle(center, 16.f, FillStyle::solid(Colors::white), StrokeStyle::solid(Colors::red, 6.f));
}

} // namespace clock_demo
