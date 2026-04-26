#include "../common/V5ExampleApp.hpp"

int main(int argc, char* argv[]) {
  return flux::examples::runV5Example(argc, argv, {
      .title = "Flux v5 Segmented Control Demo",
      .summary = "Segmented choices use scope-owned state and retained list items in the v5 shell.",
      .rows = {"Overview", "Details", "History", "Settings"},
  });
}
