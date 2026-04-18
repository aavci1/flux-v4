#pragma once

#include <cstdlib>
#include <cstring>

namespace flux::debug {

inline bool envTruthy(char const* value) {
  return value && value[0] != '\0' && std::strcmp(value, "0") != 0 &&
         std::strcmp(value, "false") != 0;
}

inline bool inputEnabled() {
  static int cached = -1;
  if (cached < 0) {
    cached = envTruthy(std::getenv("FLUX_DEBUG_INPUT")) ? 1 : 0;
  }
  return cached != 0;
}

inline bool inputVerbose() {
  static int cached = -1;
  if (cached < 0) {
    cached = envTruthy(std::getenv("FLUX_DEBUG_INPUT_VERBOSE")) ? 1 : 0;
  }
  return cached != 0;
}

inline bool layoutEnabled() {
  static int cached = -1;
  if (cached < 0) {
    cached = envTruthy(std::getenv("FLUX_DEBUG_LAYOUT")) ? 1 : 0;
  }
  return cached != 0;
}

} // namespace flux::debug
