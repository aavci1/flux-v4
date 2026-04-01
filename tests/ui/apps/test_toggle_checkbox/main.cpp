#include <Flux.hpp>
#include <Flux/Reactive/Reactive.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/Views/Checkbox.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/Toggle.hpp>
#include <Flux/UI/Views/VStack.hpp>

#include <string>

using namespace flux;

struct ToggleCheckboxRoot {
  auto body() const {
    auto wifi = useState(false);
    auto agree = useState(false);
    return VStack{
        .spacing = 24.f,
        .padding = 32.f,
        .hAlign = HorizontalAlignment::Leading,
        .children =
            {
                Text{.text = "Toggle & checkbox",
                     .font = {.size = 20.f, .weight = 700.f},
                     .color = Color::hex(0x111118)},
                HStack{
                    .spacing = 12.f,
                    .vAlign = VerticalAlignment::Center,
                    .children =
                        {
                            Text{.text = "Wi-Fi",
                                 .font = {.size = 15.f, .weight = 500.f},
                                 .color = Color::hex(0x111118)},
                            Toggle{
                                .value = wifi,
                                .testFocusKey = "wifi-toggle",
                                .onChange = [](bool) {},
                            },
                        },
                },
                HStack{
                    .spacing = 12.f,
                    .vAlign = VerticalAlignment::Center,
                    .children =
                        {
                            Text{.text = "Agree",
                                 .font = {.size = 15.f, .weight = 500.f},
                                 .color = Color::hex(0x111118)},
                            Checkbox{
                                .value = agree,
                                .testFocusKey = "agree-box",
                                .onChange = [](bool) {},
                            },
                        },
                },
                Text{
                    .text = std::string("state:") + (*wifi ? "1" : "0") + (*agree ? "1" : "0"),
                    .testFocusKey = "state-label",
                    .font = {.size = 14.f, .weight = 400.f},
                    .color = Color::hex(0x6E6E80),
                },
            },
    };
  }
};

int main(int argc, char* argv[]) {
  Application app(argc, argv);
  auto& w = app.createWindow({
      .size = {520, 360},
      .title = "Flux UI test — toggle & checkbox",
  });
  w.setView<ToggleCheckboxRoot>();
  return app.exec();
}
