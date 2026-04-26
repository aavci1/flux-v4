#include "../common/V5ExampleApp.hpp"

int main(int argc, char* argv[]) {
  return flux::examples::runV5Example(argc, argv, {
      .title = "Flux v5 Card Demo",
      .summary = "Card-style content uses one retained subtree; item order changes through keyed For rows.",
      .rows = {"Summary card", "Expandable card", "Metric card", "Action card"},
  });
}
