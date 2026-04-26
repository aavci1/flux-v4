#include "../common/V5ExampleApp.hpp"

int main(int argc, char* argv[]) {
  return flux::examples::runV5Example(argc, argv, {
      .title = "Flux v5 Button Demo",
      .summary = "Button states are expressed with retained nodes and signal-driven leaf styling.",
      .rows = {"Primary button", "Secondary button", "Icon button", "Toolbar action"},
  });
}
