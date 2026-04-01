#include <Flux.hpp>
#include <Flux/Core/Application.hpp>
#include <Flux/Reactive/Reactive.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/Views/Slider.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/VStack.hpp>

#include <sstream>
#include <string>

using namespace flux;

struct SliderTestRoot {
  auto body() const {
    auto level = useState(0.35f);
    return VStack{
        .spacing = 20.f,
        .padding = 32.f,
        .hAlign = HorizontalAlignment::Leading,
        .children =
            {
                Text{.text = "Slider test",
                     .font = {.size = 20.f, .weight = 700.f},
                     .color = Color::hex(0x111118)},
                Slider{
                    .value = level,
                    .min = 0.f,
                    .max = 1.f,
                    .testFocusKey = "level-slider",
                    .onChange =
                        [](float) {
                          Application::instance().markReactiveDirty();
                        },
                },
                Text{
                    .text = [&] {
                      std::ostringstream o;
                      o.setf(std::ios::fixed);
                      o.precision(2);
                      o << "value:" << *level;
                      return o.str();
                    }(),
                    .testFocusKey = "value-label",
                    .font = {.size = 15.f, .weight = 500.f},
                    .color = Color::hex(0x111118),
                },
            },
    };
  }
};

int main(int argc, char* argv[]) {
  Application app(argc, argv);
  auto& w = app.createWindow({
      .size = {520, 260},
      .title = "Flux UI test — slider",
  });
  w.setView<SliderTestRoot>();
  return app.exec();
}
