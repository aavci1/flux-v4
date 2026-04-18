#include <Flux.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/Reactive/AnimationClock.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/ViewModifiers.hpp>

#include <chrono>
#include <cmath>
#include <ctime>

#include "clock.hpp"

using namespace flux;

struct ClockFace : ViewModifiers<ClockFace> {
  auto body() const {
    return Render{
        .measureFn =
            [](LayoutConstraints const& c, LayoutHints const&) {
              float const s = std::min(std::isfinite(c.maxWidth) ? c.maxWidth : 360.f,
                                       std::isfinite(c.maxHeight) ? c.maxHeight : 360.f);
              return Size{s, s};
            },
        .draw =
            [](Canvas& canvas, Rect frame) {
              using namespace std::chrono;
              auto const now = system_clock::now();
              std::time_t const tt = system_clock::to_time_t(now);
              double const fracSec = duration<double>(now - system_clock::from_time_t(tt)).count();
              std::tm* local = std::localtime(&tt);
              if (!local) {
                return;
              }
              int const h12 = local->tm_hour % 12;
              float const s = static_cast<float>(local->tm_sec) + static_cast<float>(fracSec);
              float const hDeg =
                  static_cast<float>(h12) * 30.f + static_cast<float>(local->tm_min) * 0.5f + s / 120.f;
              float const mDeg = static_cast<float>(local->tm_min) * 6.f + s * 0.1f;
              float const sDeg = s * 6.f;
              clock_demo::drawClock(canvas, frame, hDeg, mDeg, sDeg);
            },
        .pure = false,
    };
  }
};

int main(int argc, char* argv[]) {
  Application app(argc, argv);
  auto& w = app.createWindow<Window>({
      .size = {800, 800},
      .title = "Flux — Clock",
      .resizable = true,
  });
  w.setView<ClockFace>();

  // Shared ~60 Hz tick: smooth second hand always moves, so each tick warrants a redraw.
  ObserverHandle const hRedraw = AnimationClock::instance().subscribe([&w](AnimationTick const&) {
    w.requestRedraw();
  });
  (void)hRedraw;

  return app.exec();
}
