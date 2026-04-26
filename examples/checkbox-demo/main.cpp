#include "../common/V5ExampleApp.hpp"

int main(int argc, char* argv[]) {
  return flux::examples::runV5Example(argc, argv, {
      .title = "Flux v5 Checkbox Demo",
      .summary = "Checkbox examples are staged as signal-backed options in a retained v5 list.",
      .rows = {"Unchecked", "Checked", "Indeterminate", "Disabled"},
  });
}
