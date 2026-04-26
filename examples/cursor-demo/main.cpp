#include "../common/V5ExampleApp.hpp"

int main(int argc, char* argv[]) {
  return flux::examples::runV5Example(argc, argv, {
      .title = "Flux v5 Cursor Demo",
      .summary = "Cursor targets keep their retained scene nodes while hover state is represented by signals.",
      .rows = {"Arrow", "Pointing hand", "I-beam", "Resize", "Crosshair"},
  });
}
