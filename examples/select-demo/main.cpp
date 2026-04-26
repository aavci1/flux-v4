#include "../common/V5ExampleApp.hpp"

int main(int argc, char* argv[]) {
  return flux::examples::runV5Example(argc, argv, {
      .title = "Flux v5 Select Demo",
      .summary = "Select options are staged as keyed rows while menu-specific behavior is rebuilt on v5.",
      .rows = {"Small", "Medium", "Large", "Custom"},
  });
}
