#include <Flux/Core/Types.hpp>
#include <Flux/Core/Utf8.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Icon.hpp>
#include <Flux/UI/Views/Text.hpp>

namespace flux {

Element Icon::body() const {
    FluxTheme const &theme = useEnvironment<FluxTheme>();

    float const sz = resolveFloat(size, theme.typeBody.size);
    float const wght = resolveFloat(weight, theme.typeBody.weight);
    Color const col = resolveColor(color, theme.colorTextPrimary);

    std::string utf8 = encodeUtf8(iconCodepoint(name));

    return Text{
               .text = std::move(utf8),
               .font = Font{
                   .family = theme.iconFontFamily,
                   .size = sz * 1.25f,
                   .weight = wght * 1.25f,
               },
               .color = col,
               .horizontalAlignment = HorizontalAlignment::Center,
               .verticalAlignment = VerticalAlignment::Center,
           }
        .frame(sz, sz);
}

} // namespace flux
