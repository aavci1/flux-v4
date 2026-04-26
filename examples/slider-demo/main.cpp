#include "../common/V5ExampleApp.hpp"

int main(int argc, char* argv[]) {
  return flux::examples::runV5Example(argc, argv, {
      .title = "Flux v5 Slider Demo",
      .summary = "Slider tracks are represented as retained reactive rows for Stage 8 build coverage.",
      .rows = {"Volume", "Brightness", "Progress", "Threshold"},
  });
}
