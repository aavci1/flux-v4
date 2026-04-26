#include "../common/V5ExampleApp.hpp"

int main(int argc, char* argv[]) {
  return flux::examples::runV5Example(argc, argv, {
      .title = "Flux v5 Text Demo",
      .summary = "Text measurement and retained text nodes are exercised through static and keyed rows.",
      .rows = {"No wrap", "Wrap", "Centered", "Footnote", "Headline"},
  });
}
