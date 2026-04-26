#include "../common/V5ExampleApp.hpp"

int main(int argc, char* argv[]) {
  return flux::examples::runV5Example(argc, argv, {
      .title = "Flux v5 Table Demo",
      .summary = "Table rows migrate to keyed v5 lists so row scopes survive reorder operations.",
      .rows = {"Account", "Owner", "Stage", "Amount", "Probability"},
  });
}
