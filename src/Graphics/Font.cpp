#include <Flux/Graphics/Font.hpp>

namespace flux {

Font TextStyle::toFont() const {
  return Font{family, size, weight, italic};
}

bool isFromTheme(TextStyle const& s) {
  return s.size < 0.f;
}

TextStyle resolveStyle(TextStyle const& override, TextStyle const& themeValue) {
  return isFromTheme(override) ? themeValue : override;
}

} // namespace flux
