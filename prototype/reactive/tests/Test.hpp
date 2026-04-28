#pragma once

#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

#define V5_CHECK(expr)                                                        \
  do {                                                                        \
    if (!(expr)) {                                                            \
      throw std::runtime_error(std::string(__FILE__) + ":" +                 \
        std::to_string(__LINE__) + ": check failed: " #expr);                \
    }                                                                         \
  } while (false)

#define V5_TEST_MAIN(fn)                                                      \
  int main() {                                                                \
    try {                                                                     \
      fn();                                                                   \
      std::cout << #fn << " passed\n";                                       \
      return EXIT_SUCCESS;                                                    \
    } catch (std::exception const& error) {                                   \
      std::cerr << error.what() << "\n";                                     \
      return EXIT_FAILURE;                                                    \
    }                                                                         \
  }
