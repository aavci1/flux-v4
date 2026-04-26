#include "../common/V5ExampleApp.hpp"

int main(int argc, char* argv[]) {
  return flux::examples::runV5Example(argc, argv, {
      .title = "Flux v5 Layout Demo",
      .summary = "Stacks, spacing, and keyed row movement validate the v5 mount-time layout path.",
      .rows = {"VStack", "HStack", "ZStack", "Padding", "Frame"},
  });
}
