#include "../common/V5ExampleApp.hpp"

int main(int argc, char* argv[]) {
  return flux::examples::runV5Example(argc, argv, {
      .title = "Flux v5 Blend Demo",
      .summary = "Blend-mode samples mount as static retained rows while reactive bindings keep layout stable.",
      .rows = {"Normal", "Multiply", "Screen", "Overlay", "Difference"},
  });
}
