#include "../common/V5ExampleApp.hpp"

int main(int argc, char* argv[]) {
  return flux::examples::runV5Example(argc, argv, {
      .title = "Flux v5 Image Demo",
      .summary = "Image examples now share the v5 retained shell while asset-backed leaves are restored later.",
      .rows = {"Fit", "Fill", "Thumbnail", "Preview", "Fallback"},
  });
}
