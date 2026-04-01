#include <Flux/Graphics/AttributedString.hpp>

namespace flux {

AttributedString AttributedString::plain(std::string_view text, Font const& font, Color const& color) {
  return {std::string(text),
          {{0, static_cast<std::uint32_t>(text.size()), font, color}}};
}

} // namespace flux
