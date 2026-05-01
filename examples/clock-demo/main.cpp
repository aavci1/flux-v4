#include <Flux.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/ViewModifiers.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/Render.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/ZStack.hpp>

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

void drawCenteredText(Canvas& canvas, std::string_view text, Font const& font, Color color,
                      float maxWidth, Point center) {
  auto layout = Application::instance().textSystem().layout(text, font, color, maxWidth);
  Size const measured = layout->measuredSize;
  canvas.drawTextLayout(*layout, {center.x - measured.width * 0.5f, center.y - measured.height * 0.5f});
}

Size measureClock(LayoutConstraints const& constraints) {
  float const width = std::isfinite(constraints.maxWidth) ? constraints.maxWidth : 640.f;
  float const height = std::isfinite(constraints.maxHeight) ? constraints.maxHeight : width;
  return Size{std::max(1.f, width), std::max(1.f, height)};
}

struct ClockGeometry {
  float topBand = 0.f;
  float bottomBand = 0.f;
  Rect clockFrame{};
  float side = 1.f;
  Point center{};
  float radius = 1.f;
};

ClockGeometry clockGeometry(Rect frame) {
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
  return ClockGeometry{
      .topBand = topBand,
      .bottomBand = bottomBand,
      .clockFrame = clockFrame,
      .side = side,
      .center = center,
      .radius = radius,
  };
}

void drawClockFace(Canvas& canvas, Rect frame, Theme const& theme) {
  ClockGeometry const geometry = clockGeometry(frame);
  Point const center = geometry.center;
  float const radius = geometry.radius;

  Color const face = theme.elevatedBackgroundColor;
  Color const ring = theme.separatorColor;
  Color const major = theme.labelColor;
  Color const minor = theme.tertiaryLabelColor;

  drawCenteredText(canvas, "Clock Demo", theme.largeTitleFont, theme.labelColor,
                   std::max(1.f, frame.width - 48.f),
                   {frame.center().x, frame.y + geometry.topBand * 0.32f});
  drawCenteredText(canvas, "spring-ticked seconds", theme.bodyFont, theme.secondaryLabelColor,
                   std::max(1.f, frame.width - 48.f),
                   {frame.center().x, frame.y + geometry.topBand * 0.66f});

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
}

struct ClockFace : ViewModifiers<ClockFace> {
  auto body() const {
    auto theme = useEnvironment<ThemeKey>();
    return Render{
        .measureFn = [](LayoutConstraints const& constraints, LayoutHints const&) {
          return measureClock(constraints);
        },
        .draw = [theme](Canvas& canvas, Rect frame) {
          drawClockFace(canvas, frame, theme.evaluate());
        },
    }
        .rasterize();
  }
};

Reactive::Bindable<float> radiansFromDegrees(Reactive::Bindable<float> degrees) {
  return Reactive::Bindable<float>{[degrees = std::move(degrees)] {
    return std::fmod(degrees.evaluate(), 360.f) * kPi / 180.f;
  }};
}

template<typename Source>
Reactive::Bindable<float> bindDegrees(Source source) {
  return Reactive::Bindable<float>{[source = std::move(source)] {
    return source.evaluate();
  }};
}

Element clockHandLayer(ClockGeometry const& geometry, Reactive::Bindable<float> radians,
                       Color color, float width, float length, float tail) {
  Point const center = geometry.center;
  return ZStack{
      .children = children(
          Rectangle{}
              .size(width, length + tail)
              .cornerRadius(width * 0.5f)
              .fill(color)
              .position(-width * 0.5f, -length)
      ),
  }
      .position(center.x, center.y)
      .rotate(std::move(radians));
}

Element centerDot(ClockGeometry const& geometry, Theme const& theme) {
  float const radius = geometry.radius * 0.055f;
  return Rectangle{}
      .size(radius * 2.f, radius * 2.f)
      .cornerRadius(radius)
      .fill(theme.dangerColor)
      .stroke(theme.elevatedBackgroundColor, 3.f)
      .position(geometry.center.x - radius, geometry.center.y - radius);
}

struct ClockHands : ViewModifiers<ClockHands> {
  auto body() const {
    auto theme = useEnvironment<ThemeKey>();
    auto clock = useState(readClock());

    double dayOffset = 0.0;
    int lastRawSecond = clock.peek().rawSecondOfDay;
    useFrame([clock, dayOffset, lastRawSecond](AnimationTick const&) mutable {
      ClockSnapshot next = readClock(dayOffset);
      if (next.rawSecondOfDay < lastRawSecond) {
        dayOffset += 24.0 * 60.0 * 60.0;
        next = readClock(dayOffset);
      }
      if (next.rawSecondOfDay != lastRawSecond) {
        lastRawSecond = next.rawSecondOfDay;
        clock = std::move(next);
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

    LayoutConstraints const* constraints = useLayoutConstraints();
    Size const measured = constraints ? measureClock(*constraints) : Size{640.f, 640.f};
    Rect const frame{0.f, 0.f, measured.width, measured.height};
    ClockGeometry const geometry = clockGeometry(frame);
    Theme const initialTheme = theme();
    float const side = geometry.side;
    float const radius = geometry.radius;
    float const hourWidth = std::max(5.f, side * 0.018f);
    float const minuteWidth = std::max(4.f, side * 0.013f);
    float const secondWidth = std::max(2.f, side * 0.006f);

    return ZStack{
        .children = children(
            clockHandLayer(geometry, radiansFromDegrees(bindDegrees(hour)),
                           initialTheme.labelColor, hourWidth, radius * 0.42f, radius * 0.08f),
            clockHandLayer(geometry, radiansFromDegrees(bindDegrees(minute)),
                           initialTheme.labelColor, minuteWidth, radius * 0.64f, radius * 0.10f),
            clockHandLayer(geometry, radiansFromDegrees(bindDegrees(second)),
                           initialTheme.dangerColor, secondWidth, radius * 0.72f, radius * 0.16f),
            centerDot(geometry, initialTheme),
            Text{
                .text = Reactive::Bindable<std::string>{[clock] {
                  return clock.evaluate().label;
                }},
                .font = initialTheme.monospacedBodyFont,
                .color = Reactive::Bindable<Color>{[theme] {
                  return theme.evaluate().secondaryLabelColor;
                }},
                .horizontalAlignment = HorizontalAlignment::Center,
                .verticalAlignment = VerticalAlignment::Center,
            }
                .size(std::max(1.f, frame.width - 48.f), geometry.bottomBand)
                .position(24.f, frame.height - geometry.bottomBand * 0.98f)
        ),
    }
        .flex(1.f);
  }
};

struct Clock : ViewModifiers<Clock> {
  auto body() const {
    return ZStack{
        .children = children(
            ClockFace{},
            ClockHands{}.flex(1.f)
        ),
    }
        .flex(1.f);
  }
};

struct ClockDemoRoot {
  auto body() const {
    auto theme = useEnvironment<ThemeKey>();
    return Clock{}
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
