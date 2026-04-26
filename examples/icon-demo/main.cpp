#include "../common/V5ExampleApp.hpp"

int main(int argc, char* argv[]) {
  return flux::examples::runV5Example(argc, argv, {
      .title = "Flux v5 Icon Demo",
      .summary = "Icon catalog rows mount once and remain stable through keyed reordering.",
      .rows = {"Search", "Settings", "Download", "Favorite", "Warning"},
  });
}
