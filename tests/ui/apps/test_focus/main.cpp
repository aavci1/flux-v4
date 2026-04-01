#include <Flux.hpp>
#include <Flux/Core/Application.hpp>
#include <Flux/Reactive/Reactive.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/TextInput.hpp>
#include <Flux/UI/Views/VStack.hpp>

#include <string>

using namespace flux;

struct FocusTestRoot {
  auto body() const {
    auto a = useState(std::string(""));
    auto b = useState(std::string(""));
    return VStack{
        .spacing = 14.f,
        .padding = 28.f,
        .hAlign = HorizontalAlignment::Leading,
        .children =
            {
                Text{.text = "Focus test",
                     .font = {.size = 20.f, .weight = 700.f},
                     .color = Color::hex(0x111118)},
                TextInput{
                    .value = a,
                    .testFocusKey = "field-a",
                    .placeholder = "Field A",
                    .onChange =
                        [](std::string const&) {
                          Application::instance().markReactiveDirty();
                        },
                },
                TextInput{
                    .value = b,
                    .testFocusKey = "field-b",
                    .placeholder = "Field B",
                    .onChange =
                        [](std::string const&) {
                          Application::instance().markReactiveDirty();
                        },
                },
                Text{
                    .text = std::string("out:") + *a + "|" + *b,
                    .testFocusKey = "combined-label",
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
      .size = {520, 340},
      .title = "Flux UI test — focus",
  });
  w.setView<FocusTestRoot>();
  return app.exec();
}
