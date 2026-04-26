#include "Effect.hpp"
#include "Signal.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>

using namespace fluxv5;

int main() {
  constexpr int iterations = 1'000'000;
  Signal<int> signal(0);
  int sink = 0;

  Effect effect([&] {
    sink += signal.get();
  });

  signal.set(1);

  auto start = std::chrono::steady_clock::now();
  for (int i = 2; i < iterations + 2; ++i) {
    signal.set(i);
  }
  auto end = std::chrono::steady_clock::now();

  auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
  double nsPerEffect = static_cast<double>(elapsed.count()) / iterations;

  std::cout << "effect_fire_ns=" << nsPerEffect << "\n";
  std::cout << "sink=" << sink << "\n";

  return nsPerEffect < 500.0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
