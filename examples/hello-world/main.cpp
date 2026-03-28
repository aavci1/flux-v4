#include <Flux.hpp>
#include <Flux/UI/UI.hpp>

using namespace flux;

int main(int argc, char* argv[]) {
  Application app(argc, argv);

  auto& w = app.createWindow<Window>({
      .size = {400, 400},
      .title = "Hello, World!",
  });

  w.setView(
    Text {
      .text = "Hello, World!",
      .font = {.size = 32.f, .weight = 500.f},
      .color = Colors::darkGray,
    }
  );

  return app.exec();
}
