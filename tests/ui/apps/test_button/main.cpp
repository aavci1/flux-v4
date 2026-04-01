#include <Flux.hpp>
#include <Flux/Core/Application.hpp>
#include <Flux/Reactive/Reactive.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/Views/Button.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/VStack.hpp>

#include <string>

using namespace flux;

struct ButtonTestRoot {
  auto body() const {
    auto count = useState(0);
    return VStack{
        .spacing = 20.f,
        .padding = 32.f,
        .hAlign = HorizontalAlignment::Leading,
        .children =
            {
                Text{.text = "Button test",
                     .font = {.size = 20.f, .weight = 700.f},
                     .color = Color::hex(0x111118)},
                Button{
                    .label = "Tap me",
                    .testFocusKey = "tap-me",
                    .onTap =
                        [count] {
                          count = *count + 1;
                          Application::instance().markReactiveDirty();
                        },
                },
                Text{
                    .text = std::string("count:") + std::to_string(*count),
                    .testFocusKey = "count-label",
                    .font = {.size = 16.f, .weight = 500.f},
                    .color = Color::hex(0x111118),
                },
            },
    };
  }
};

int main(int argc, char* argv[]) {
  Application app(argc, argv);
  auto& w = app.createWindow({
      .size = {480, 320},
      .title = "Flux UI test — button",
  });
  w.setView<ButtonTestRoot>();
  return app.exec();
}
