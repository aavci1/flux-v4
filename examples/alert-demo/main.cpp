#include "../common/V5ExampleApp.hpp"

int main(int argc, char* argv[]) {
  return flux::examples::runV5Example(argc, argv, {
      .title = "Flux v5 Alert Demo",
      .summary = "Alert surfaces are represented as retained v5 content with scope-owned actions.",
      .rows = {"Confirmation copy", "Destructive action", "Secondary action", "Dismiss affordance"},
  });
}
