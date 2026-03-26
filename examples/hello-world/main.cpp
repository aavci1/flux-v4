#include <Flux.hpp>

using namespace flux;

int main(int argc, char* argv[]) {
  Application app(argc, argv);

  auto& window = app.createWindow({
      .size = {400, 400},
      .title = "Flux"
  });

  return app.exec();
}