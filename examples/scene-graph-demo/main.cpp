#include "../common/V5ExampleApp.hpp"

int main(int argc, char* argv[]) {
  return flux::examples::runV5Example(argc, argv, {
      .title = "Flux v5 Scene Graph Demo",
      .summary = "Scene graph smoke content verifies retained nodes, child order, and redraw requests.",
      .rows = {"GroupNode", "RectNode", "TextNode", "Prepared ops", "Dirty propagation"},
  });
}
