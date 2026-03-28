#include <Flux.hpp>

#include <chrono>
#include <cmath>
#include <ctime>

#include "clock.hpp"

using namespace flux;

namespace {

/// Choose the next target >= `current` that matches `targetDeg` modulo 360° so hands always move
/// forward (handles 59 → 0 second, etc.).
inline float unwrapForward(float current, float targetDeg) {
  float t = std::fmod(targetDeg, 360.f);
  if (t < 0.f) {
    t += 360.f;
  }
  float T = t;
  while (T < current) {
    T += 360.f;
  }
  return T;
}

class ClockWindow : public Window {
  Animated<float> hourAngle_{0.f};
  Animated<float> minuteAngle_{0.f};
  Animated<float> secondAngle_{0.f};
  bool firstSync_{true};

public:
  explicit ClockWindow(WindowConfig const& c) : Window(c) {
    auto redraw = [this]() { Window::requestRedraw(); };
    hourAngle_.observe(redraw);
    minuteAngle_.observe(redraw);
    secondAngle_.observe(redraw);
  }

  void render(Canvas& c) override {
    const auto sz = getSize();
    Rect bounds =
        Rect::sharp(0.f, 0.f, static_cast<float>(sz.width), static_cast<float>(sz.height));
    c.clear(Color::rgb(245, 245, 248));
    clock_demo::drawClock(c, bounds, hourAngle_.get(), minuteAngle_.get(), secondAngle_.get());
  }

  void syncTimeFromSystem() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm* local = std::localtime(&t);
    if (!local) {
      return;
    }
    const int hour24 = local->tm_hour;
    const int minutes = local->tm_min;
    const int seconds = local->tm_sec;

    const int hours12 = hour24 % 12;
    const float h = static_cast<float>(hours12) * 30.f + static_cast<float>(minutes) * 0.5f;
    const float m = static_cast<float>(minutes) * 6.f + static_cast<float>(seconds) * 0.1f;
    const float s = static_cast<float>(seconds) * 6.f;

    if (firstSync_) {
      hourAngle_.set(h, Transition::instant());
      minuteAngle_.set(m, Transition::instant());
      secondAngle_.set(s, Transition::instant());
      firstSync_ = false;
      return;
    }

    {
      WithTransition wt{Transition::spring(500.f, 25.f, 0.65f)};
      hourAngle_.set(unwrapForward(hourAngle_.get(), h));
      minuteAngle_.set(unwrapForward(minuteAngle_.get(), m));
      secondAngle_.set(unwrapForward(secondAngle_.get(), s));
    }
  }
};

} // namespace

int main(int argc, char* argv[]) {
  Application app(argc, argv);

  auto& window = app.createWindow<ClockWindow>({
      .size = {800, 800},
      .title = "Clock",
  });
  const unsigned int windowHandle = window.handle();

  window.syncTimeFromSystem();
  window.requestRedraw();

  // Must filter by timer id: `AnimationClock` also uses `scheduleRepeatingTimer` (~60Hz, windowHandle 0).
  // Without this, every animation tick would call `syncTimeFromSystem()` and restart the hand animation.
  std::uint64_t const clockTimerId = app.scheduleRepeatingTimer(std::chrono::seconds(1), windowHandle);

  app.eventQueue().on<TimerEvent>([&window, windowHandle, clockTimerId](TimerEvent const& e) {
    if (e.timerId != clockTimerId) {
      return;
    }
    if (e.windowHandle != 0 && e.windowHandle != windowHandle) {
      return;
    }
    window.syncTimeFromSystem();
    Window::postRedraw(windowHandle);
  });

  return app.exec();
}
