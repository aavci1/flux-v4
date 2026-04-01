#pragma once

/// \file Flux/UI/ComponentKeyUtil.hpp
///
/// Part of the Flux public API.


#include <Flux/UI/ComponentKey.hpp>

#include <cstddef>

namespace flux {

/// True when the first `min(a.size(), b.size())` elements of \p a and \p b match
/// (one key is a prefix of the other along the shared length).
bool keySharesPrefix(ComponentKey const& a, ComponentKey const& b) noexcept;

} // namespace flux
