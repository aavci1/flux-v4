#include "../common/V5ExampleApp.hpp"

int main(int argc, char* argv[]) {
  return flux::examples::runV5Example(argc, argv, {
      .title = "Flux v5 Text Input Demo",
      .summary = "Text input chrome is represented by retained rows while editing internals are rewired.",
      .rows = {"Plain field", "Search field", "Password field", "Validation state"},
  });
}
