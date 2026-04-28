#include "EnvironmentKeyTestSupport.hpp"

namespace flux::tests {

std::uint16_t sharedEnvironmentTestKeyIndexFromOtherTranslationUnit() {
  return EnvironmentKey<flux::SharedEnvironmentTestKey>::slot().index();
}

} // namespace flux::tests
