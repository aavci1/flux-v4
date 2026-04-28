#pragma once

#include <Flux/UI/Environment.hpp>

namespace flux {

FLUX_DEFINE_ENVIRONMENT_KEY(SharedEnvironmentTestKey, int, 17);

} // namespace flux

namespace flux::tests {

std::uint16_t sharedEnvironmentTestKeyIndexFromOtherTranslationUnit();

} // namespace flux::tests
