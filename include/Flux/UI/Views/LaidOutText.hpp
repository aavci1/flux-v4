#pragma once

#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/TextLayout.hpp>

#include <memory>

namespace flux {

/// Pre-shaped text from `TextSystem::layout`; `origin` is overridden by the layout child frame when set.
struct LaidOutText {
  std::shared_ptr<TextLayout> layout;
  Point origin{};
};

} // namespace flux
