#include <Flux.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/ViewModifiers.hpp>
#include <Flux/UI/Views/Render.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <string_view>

using namespace flux;

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr double kSecondHandSettleSeconds = 0.46;

struct ClockSnapshot {
  int rawSecondOfDay = 0;
  double seconds = 0.0;
  std::string label = "00:00:00";

  bool operator==(ClockSnapshot const&) const = default;
};

Color withAlpha(Color c, float alpha) {
  c.a = alpha;
  return c;
}

ClockSnapshot readClock(double dayOffset = 0.0) {
  using namespace std::chrono;

  auto const now = system_clock::now();
  std::time_t const nowTime = system_clock::to_time_t(now);
  std::tm local{};
#if defined(_WIN32)
  localtime_s(&local, &nowTime);
#else
  localtime_r(&nowTime, &local);
#endif

  char buffer[16]{};
  std::strftime(buffer, sizeof(buffer), "%H:%M:%S", &local);

  int const raw = local.tm_hour * 3600 + local.tm_min * 60 + local.tm_sec;
  return ClockSnapshot{
      .rawSecondOfDay = raw,
      .seconds = static_cast<double>(raw) + dayOffset,
      .label = buffer,
  };
}

Point polar(Point center, float radius, float degrees) {
  float const radians = degrees * kPi / 180.f;
  return Point{
      center.x + std::sin(radians) * radius,
      center.y - std::cos(radians) * radius,
  };
}

void drawHand(Canvas& canvas, Point center, float degrees, float length, float tail,
              StrokeStyle stroke) {
  canvas.save();
  canvas.rotate(std::fmod(degrees, 360.f) * kPi / 180.f, center);
  canvas.drawLine({center.x, center.y + tail}, {center.x, center.y - length}, stroke);
  canvas.restore();
}

void drawCenteredText(Canvas& canvas, std::string_view text, Font const& font, Color color,
                      float maxWidth, Point center) {
  auto layout = Application::instance().textSystem().layout(text, font, color, maxWidth);
  Size const measured = layout->measuredSize;
  canvas.drawTextLayout(*layout, {center.x - measured.width * 0.5f, center.y - measured.height * 0.5f});
}

void drawClock(Canvas& canvas, Rect frame, Theme const& theme, float hourDegrees,
               float minuteDegrees, float secondDegrees, std::string_view timeLabel) {
  float const topBand = std::clamp(frame.height * 0.15f, 72.f, 128.f);
  float const bottomBand = std::clamp(frame.height * 0.10f, 48.f, 84.f);
  Rect const clockFrame{
      frame.x,
      frame.y + topBand,
      frame.width,
      std::max(1.f, frame.height - topBand - bottomBand),
  };

  float const side = std::max(1.f, std::min(clockFrame.width, clockFrame.height));
  Point const center = clockFrame.center();
  float const radius = side * 0.45f;

  Color const face = theme.elevatedBackgroundColor;
  Color const ring = theme.separatorColor;
  Color const major = theme.labelColor;
  Color const minor = theme.tertiaryLabelColor;
  Color const accent = theme.dangerColor;

  drawCenteredText(canvas, "Clock Demo", theme.largeTitleFont, theme.labelColor,
                   std::max(1.f, frame.width - 48.f),
                   {frame.center().x, frame.y + topBand * 0.32f});
  drawCenteredText(canvas, "spring-ticked seconds", theme.bodyFont, theme.secondaryLabelColor,
                   std::max(1.f, frame.width - 48.f),
                   {frame.center().x, frame.y + topBand * 0.66f});

  canvas.drawCircle(center, radius + 14.f, FillStyle::solid(withAlpha(theme.accentColor, 0.07f)),
                    StrokeStyle::none());
  canvas.drawCircle(center, radius, FillStyle::solid(face),
                    StrokeStyle::solid(withAlpha(ring, 0.95f), 2.f));
  canvas.drawCircle(center, radius * 0.76f, FillStyle::none(),
                    StrokeStyle::solid(withAlpha(ring, 0.45f), 1.f));

  for (int i = 0; i < 60; ++i) {
    bool const isHour = (i % 5) == 0;
    float const degrees = static_cast<float>(i) * 6.f;
    float const outer = radius - 16.f;
    float const inner = radius - (isHour ? 42.f : 28.f);
    StrokeStyle stroke = StrokeStyle::solid(isHour ? major : minor, isHour ? 3.f : 1.25f);
    stroke.cap = StrokeCap::Round;
    canvas.drawLine(polar(center, inner, degrees), polar(center, outer, degrees), stroke);
  }

  Font const numberFont{.size = 24.f, .weight = 650.f};
  struct NumberMark {
    char const* text;
    float degrees;
  };
  NumberMark const numbers[]{{"12", 0.f}, {"3", 90.f}, {"6", 180.f}, {"9", 270.f}};
  for (NumberMark const& mark : numbers) {
    auto layout = Application::instance().textSystem().layout(
        mark.text, numberFont, theme.secondaryLabelColor, 0.f);
    Size const measured = layout->measuredSize;
    Point const p = polar(center, radius * 0.62f, mark.degrees);
    canvas.drawTextLayout(*layout, {p.x - measured.width * 0.5f, p.y - measured.height * 0.5f});
  }

  StrokeStyle hourStroke = StrokeStyle::solid(major, std::max(5.f, side * 0.018f));
  hourStroke.cap = StrokeCap::Round;
  StrokeStyle minuteStroke = StrokeStyle::solid(major, std::max(4.f, side * 0.013f));
  minuteStroke.cap = StrokeCap::Round;
  StrokeStyle secondStroke = StrokeStyle::solid(accent, std::max(2.f, side * 0.006f));
  secondStroke.cap = StrokeCap::Round;

  drawHand(canvas, center, hourDegrees, radius * 0.42f, radius * 0.08f, hourStroke);
  drawHand(canvas, center, minuteDegrees, radius * 0.64f, radius * 0.10f, minuteStroke);
  drawHand(canvas, center, secondDegrees, radius * 0.72f, radius * 0.16f, secondStroke);

  canvas.drawCircle(center, radius * 0.055f, FillStyle::solid(accent),
                    StrokeStyle::solid(face, 3.f));

  drawCenteredText(canvas, timeLabel, theme.monospacedBodyFont, theme.secondaryLabelColor,
                   std::max(1.f, frame.width - 48.f),
                   {frame.center().x, frame.y + frame.height - bottomBand * 0.48f});
}

struct ClockFace : ViewModifiers<ClockFace> {
  auto body() const {
    auto theme = useEnvironment<ThemeKey>();
    auto clock = useState(readClock());

    double dayOffset = 0.0;
    double redrawUntil = 0.0;
    int lastRawSecond = clock.peek().rawSecondOfDay;
    useFrame([clock, dayOffset, redrawUntil, lastRawSecond](AnimationTick const& tick) mutable {
      ClockSnapshot next = readClock(dayOffset);
      if (next.rawSecondOfDay < lastRawSecond) {
        dayOffset += 24.0 * 60.0 * 60.0;
        next = readClock(dayOffset);
      }
      if (next.rawSecondOfDay != lastRawSecond) {
        lastRawSecond = next.rawSecondOfDay;
        clock = std::move(next);
        redrawUntil = tick.nowSeconds + kSecondHandSettleSeconds;
        Application::instance().requestRedraw();
      } else if (tick.nowSeconds < redrawUntil) {
        Application::instance().requestRedraw();
      }
    });

    auto secondTarget = [clock] {
      return static_cast<float>(clock().seconds * 6.0);
    };
    auto minuteTarget = [clock] {
      return static_cast<float>(clock().seconds * 0.1);
    };
    auto hourTarget = [clock] {
      return static_cast<float>(clock().seconds / 120.0);
    };
    auto secondSpring = [] {
      return Transition::spring(980.f, 16.f, 0.46f);
    };
    auto slowHandSpring = [] {
      return Transition::spring(360.f, 28.f, 0.36f);
    };

    auto second = useAnimation(secondTarget, secondSpring);
    auto minute = useAnimation(minuteTarget, slowHandSpring);
    auto hour = useAnimation(hourTarget, slowHandSpring);

    return Render{
        .measureFn = [](LayoutConstraints const& constraints, LayoutHints const&) {
          float const width = std::isfinite(constraints.maxWidth) ? constraints.maxWidth : 640.f;
          float const height = std::isfinite(constraints.maxHeight) ? constraints.maxHeight : width;
          return Size{std::max(1.f, width), std::max(1.f, height)};
        },
        .draw = [theme, clock, hour, minute, second](Canvas& canvas, Rect frame) {
          drawClock(canvas, frame, theme(), hour.peek(), minute.peek(), second.peek(),
                    clock.peek().label);
        },
        .pure = false,
    }
        .flex(1.f);
  }
};

struct ClockDemoRoot {
  auto body() const {
    auto theme = useEnvironment<ThemeKey>();
    return ClockFace{}
        .padding(theme().space5)
        .fill(theme().windowBackgroundColor);
  }
};

} // namespace

int main(int argc, char* argv[]) {
  Application app(argc, argv);

  auto& w = app.createWindow<Window>({
      .size = {760.f, 760.f},
      .title = "Flux - Clock demo",
  });

  w.setView<ClockDemoRoot>();

  return app.exec();
}
