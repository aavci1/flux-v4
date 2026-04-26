#include "../common/V5ExampleApp.hpp"

int main(int argc, char* argv[]) {
  return flux::examples::runV5Example(argc, argv, {
      .title = "Flux v5 Typography Demo",
      .summary = "Typography roles render through retained text nodes and theme-backed font tokens.",
      .rows = {"Large title", "Title", "Headline", "Body", "Caption"},
  });
}
