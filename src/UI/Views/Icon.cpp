#include <Flux/Core/Types.hpp>
#include <Flux/Core/Utf8.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Icon.hpp>
#include <Flux/UI/Views/Text.hpp>

namespace flux {

Element Icon::body() const {
    Theme const& theme = useEnvironment<Theme>();

    float const s = resolveFloat(size, theme.fontBody.size);
    float const w = resolveFloat(weight, theme.fontBody.weight);
    Color const c = resolveColor(color, theme.colorTextPrimary);

    std::string utf8 = encodeUtf8(static_cast<char32_t>(name));

    return Text {
        .text = std::move(utf8),
        .font = Font {
            .family = theme.iconFontFamily,
            .size = s,
            .weight = w,
        },
        .color = c,
        .horizontalAlignment = HorizontalAlignment::Center,
        .verticalAlignment = VerticalAlignment::Center,
    }.size(s, s);
}

} // namespace flux
