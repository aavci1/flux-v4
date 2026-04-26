#include <Flux.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/VStack.hpp>
#include <Flux/UI/Views/ZStack.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using namespace flux;

namespace {

Color alpha(Color color, float opacity) {
  return Color{color.r, color.g, color.b, opacity};
}

struct AnimationDemoRoot {
  Element body() const {
    Theme const& theme = useEnvironment<Theme>();
    auto phase = useAnimation<float>(
        0.f,
        AnimationOptions{
            .transition = Transition::linear(1.4f),
            .repeat = AnimationOptions::kRepeatForever,
            .autoreverse = true,
        });

    if (!phase.isRunning()) {
      phase.play(1.f, AnimationOptions{
                         .transition = Transition::linear(1.4f),
                         .repeat = AnimationOptions::kRepeatForever,
                         .autoreverse = true,
                     });
    }

    std::vector<Element> bars;
    bars.reserve(5);
    for (int i = 0; i < 5; ++i) {
      float const anchor = static_cast<float>(i) / 4.f;
      float const emphasis = std::clamp(1.f - std::abs(*phase - anchor) * 2.8f, 0.f, 1.f);
      bars.push_back(Rectangle{}
                         .size(24.f, 22.f + emphasis * 42.f)
                         .cornerRadius(12.f)
                         .fill(alpha(Color::accent(), 0.35f + emphasis * 0.65f)));
    }

    return VStack{
        .spacing = theme.space5,
        .alignment = Alignment::Center,
        .children = children(
            Text{
                .text = "Flux v5 Animation",
                .font = Font::largeTitle(),
                .color = Color::primary(),
                .horizontalAlignment = HorizontalAlignment::Center,
            },
            Text{
                .text = "useAnimation returns a Scope-owned handle; reactive leaf bindings are mounted once.",
                .font = Font::body(),
                .color = Color::secondary(),
                .horizontalAlignment = HorizontalAlignment::Center,
                .wrapping = TextWrapping::Wrap,
            }.width(460.f),
            ZStack{
                .horizontalAlignment = Alignment::Center,
                .verticalAlignment = Alignment::Center,
                .children = children(
                    Rectangle{}
                        .size(520.f, 220.f)
                        .fill(Color::controlBackground())
                        .stroke(Color::separator(), 1.f)
                        .cornerRadius(theme.radiusXLarge),
                    HStack{
                        .spacing = theme.space3,
                        .alignment = Alignment::Center,
                        .children = std::move(bars),
                    })})}
        .padding(theme.space7)
        .fill(Color::windowBackground())
        .environment(theme);
  }
};

} // namespace

int main(int argc, char* argv[]) {
  Application app(argc, argv);

  auto& window = app.createWindow<Window>({
      .size = {760, 520},
      .title = "Flux v5 Animation Demo",
      .resizable = true,
  });

  window.setView<AnimationDemoRoot>();
  return app.exec();
}
