#include "../common/V5ExampleApp.hpp"

int main(int argc, char* argv[]) {
  return flux::examples::runV5Example(argc, argv, {
      .title = "Flux v5 Toggle Demo",
      .summary = "Toggle examples use retained rows and signal-driven visual state in the v5 runtime.",
      .rows = {"Wi-Fi", "Bluetooth", "Notifications", "Reduced motion"},
  });
}
