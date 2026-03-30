#pragma once

#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/Font.hpp>

namespace flux {

/**
 * Resolved row height for single-line inputs (\ref TextInput, \ref Picker trigger, etc.).
 *
 * When `explicitHeight` is 0, uses the same rule as \ref TextInput: measured line ("Agy") +
 * `2 * paddingV` + selection slack. When `explicitHeight` > 0, returns
 * `max(explicitHeight, that minimum)` so custom heights never clip the text line.
 */
float resolvedInputFieldHeight(Font const& font, Color textInkColor, float paddingV, float explicitHeight);

} // namespace flux
