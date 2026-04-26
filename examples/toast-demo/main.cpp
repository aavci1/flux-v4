#include "../common/V5ExampleApp.hpp"

int main(int argc, char* argv[]) {
  return flux::examples::runV5Example(argc, argv, {
      .title = "Flux v5 Toast Demo",
      .summary = "Toast scenarios mount in a retained shell with signal-backed ordering controls.",
      .rows = {"Success", "Warning", "Error", "Undo action"},
  });
}
