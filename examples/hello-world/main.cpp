#include <Flux.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/UI.hpp>

using namespace flux;

struct HelloRoot {
  auto body() const {
    auto theme = useEnvironment<ThemeKey>();
    return Text{
        .text = "Hello, World!",
        .font = Font::largeTitle(),
        .color = Color::primary(),
        .horizontalAlignment = HorizontalAlignment::Center,
        .verticalAlignment = VerticalAlignment::Center,
    };
  }
};

int main(int argc, char* argv[]) {
  Application app(argc, argv);

  auto& w = app.createWindow<Window>({
      .size = {800, 800},
      .title = "Hello, World!",
  });

  w.setView<HelloRoot>();

  return app.exec();
}
