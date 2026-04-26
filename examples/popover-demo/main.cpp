#include "../common/V5ExampleApp.hpp"

int main(int argc, char* argv[]) {
  return flux::examples::runV5Example(argc, argv, {
      .title = "Flux v5 Popover Demo",
      .summary = "Popover scenarios are represented as retained trigger rows for the v5 migration pass.",
      .rows = {"Bottom placement", "Top placement", "Trailing placement", "Dismiss on tap"},
  });
}
