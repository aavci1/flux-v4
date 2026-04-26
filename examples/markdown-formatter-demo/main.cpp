#include "../common/V5ExampleApp.hpp"

int main(int argc, char* argv[]) {
  return flux::examples::runV5Example(argc, argv, {
      .title = "Flux v5 Markdown Formatter",
      .summary = "Formatter sections render through the retained v5 text and list primitives.",
      .rows = {"Heading", "Paragraph", "Inline code", "Quote", "List"},
  });
}
