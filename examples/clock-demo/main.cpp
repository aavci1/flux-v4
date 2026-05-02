#include "ClockViews.hpp"

#include <Flux.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/VStack.hpp>

#include <string>

using namespace flux;

namespace {

struct ClockDemoRoot {
  auto body() const {
    auto theme = useEnvironment<ThemeKey>();
    auto clock = clock_demo::useClock();
    Theme const currentTheme = theme();

    return VStack{
        .spacing = currentTheme.space3,
        .alignment = Alignment::Stretch,
        .children = children(
            Text{
                .text = "Clock Demo",
                .font = currentTheme.largeTitleFont,
                .color = currentTheme.labelColor,
                .horizontalAlignment = HorizontalAlignment::Center,
            },
            Text{
                .text = "spring-ticked seconds",
                .font = currentTheme.bodyFont,
                .color = currentTheme.secondaryLabelColor,
                .horizontalAlignment = HorizontalAlignment::Center,
            },
            clock_demo::Clock{.clock = clock}.flex(1.f, 1.f, 0.f),
            Text{
                .text = Reactive::Bindable<std::string>{[clock] {
                  return clock.evaluate().label;
                }},
                .font = currentTheme.monospacedBodyFont,
                .color = Reactive::Bindable<Color>{[theme] {
                  return theme.evaluate().secondaryLabelColor;
                }},
                .horizontalAlignment = HorizontalAlignment::Center,
                .maxLines = 1,
            }
        ),
    }
        .padding(currentTheme.space5)
        .fill(currentTheme.windowBackgroundColor);
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
