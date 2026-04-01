#pragma once

/// \file Flux/Core/Utf8.hpp
///
/// Part of the Flux public API.


#include <cstdint>
#include <string>

namespace flux {

/// UTF-8 encode a single Unicode scalar value (BMP or supplementary).
std::string encodeUtf8(char32_t cp);

} // namespace flux
