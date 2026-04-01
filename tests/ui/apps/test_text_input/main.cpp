#include <Flux.hpp>
#include <Flux/Core/Application.hpp>
#include <Flux/Reactive/Reactive.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/TextInput.hpp>
#include <Flux/UI/Views/VStack.hpp>

#include <string>

using namespace flux;

struct TextInputTestRoot {
  auto body() const {
    auto name = useState(std::string(""));
    return VStack{
        .spacing = 16.f,
        .padding = 28.f,
        .hAlign = HorizontalAlignment::Leading,
        .children =
            {
                Text{.text = "Text input test",
                     .font = {.size = 20.f, .weight = 700.f},
                     .color = Color::hex(0x111118)},
                TextInput{
                    .value = name,
                    .testFocusKey = "name-field",
                    .placeholder = "Type here",
                    .onChange =
                        [](std::string const&) {
                          Application::instance().markReactiveDirty();
                        },
                },
                Text{
                    .text = std::string("echo:") + *name,
                    .testFocusKey = "echo-label",
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
      .size = {520, 280},
      .title = "Flux UI test — text input",
  });
  w.setView<TextInputTestRoot>();
  return app.exec();
}
