#pragma once

/// \file Flux/UI/Views/LaidOutText.hpp
///
/// Part of the Flux public API.


#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/TextLayout.hpp>
#include <Flux/UI/ViewModifiers.hpp>

#include <memory>

namespace flux {

/// Pre-shaped text from `TextSystem::layout`; `origin` is overridden by the layout child frame when set.
struct LaidOutText : ViewModifiers<LaidOutText> {
  std::shared_ptr<TextLayout> layout;
  Point origin{};
};

} // namespace flux
