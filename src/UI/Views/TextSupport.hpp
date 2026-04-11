#pragma once

#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/Font.hpp>
#include <Flux/Graphics/TextLayoutOptions.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Text.hpp>

#include <utility>

namespace flux::text_detail {

inline std::pair<Font, Color> resolveBodyTextStyle(Font const &font, Color color) {
    Theme const &theme = useEnvironment<Theme>();
    return {
        resolveFont(font, theme.fontBody),
        resolveColor(color, theme.colorTextPrimary),
    };
}

inline TextLayoutOptions makeTextLayoutOptions(Text const &text) {
    TextLayoutOptions o {};
    o.horizontalAlignment = text.horizontalAlignment;
    o.verticalAlignment = text.verticalAlignment;
    o.wrapping = text.wrapping;
    o.lineHeight = 0.f;
    o.lineHeightMultiple = 0.f;
    o.maxLines = text.maxLines;
    o.firstBaselineOffset = text.firstBaselineOffset;
    return o;
}

inline TextLayoutOptions makeTextLayoutOptions(TextWrapping wrapping, float lineHeight = 0.f) {
    TextLayoutOptions o {};
    o.wrapping = wrapping;
    o.lineHeight = lineHeight;
    return o;
}

} // namespace flux::text_detail
