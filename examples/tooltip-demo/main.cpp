#include "../common/V5ExampleApp.hpp"

int main(int argc, char* argv[]) {
  return flux::examples::runV5Example(argc, argv, {
      .title = "Flux v5 Tooltip Demo",
      .summary = "Tooltip targets are staged as retained rows while overlay behavior is completed on v5.",
      .rows = {"Plain tooltip", "Icon tooltip", "Delayed tooltip", "Disabled target"},
  });
}
