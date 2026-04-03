#include <Flux.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/UI.hpp>

using namespace flux;

struct HelloRoot {
  auto body() const {
    Theme const& theme = useEnvironment<Theme>();
    return Text {
        .text = "Hello, World!",
        .style = theme.typeDisplay,
        .color = theme.colorTextSecondary
    }.padding(48.f);
  }
};

int main(int argc, char* argv[]) {
  Application app(argc, argv);

  auto& w = app.createWindow<Window>({
      .title = "Hello, World!",
  });

  w.setView<HelloRoot>();

  return app.exec();
}
