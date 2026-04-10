#include <Flux/UI/Views/TextArea.hpp>

#include <Flux/UI/Views/ScrollView.hpp>
#include <Flux/UI/Theme.hpp>

namespace flux {

TextArea::Style resolveStyle(TextArea::Style const& style, Theme const& theme) {
    return TextArea::Style {
        .font = resolveFont(style.font, theme.textAreaFont),
        .backgroundColor = resolveColor(style.backgroundColor, theme.textAreaBackgroundColor),
        .borderColor = resolveColor(style.borderColor, theme.textAreaBorderColor),
        .borderWidth = resolveFloat(style.borderWidth, theme.textAreaBorderWidth),
        .cornerRadius = resolveFloat(style.cornerRadius, theme.textAreaCornerRadius),
        .padding = style.padding.isZero() ? EdgeInsets{8.f, 12.f, 8.f, 12.f} : style.padding,
        .textColor = resolveColor(style.textColor, theme.textAreaTextColor),
        .placeholderColor = resolveColor(style.placeholderColor, theme.textAreaPlaceholderColor),
        .caretColor = resolveColor(style.caretColor, theme.textAreaCaretColor)
    };
}

Element TextArea::body() const {
    auto const& theme = useEnvironment<Theme>();
    auto [
        font,
        backgroundColor,
        borderColor,
        borderWidth,
        cornerRadius,
        padding,
        textColor,
        placeholderColor,
        caretColor
    ] = resolveStyle(style, theme);

    auto const& plainFormatter = [](std::string_view const &s, Font const& f, Color const& c) { return AttributedString::plain(s, f, c); };

    auto const& s = (*value).size() == 0 ? plainFormatter(placeholder, font, placeholderColor) : (
        formatter.has_value() ? formatter.value()(*value) : plainFormatter(*value, font, textColor)
    );

    return ScrollView {
      .axis = wrapping == TextWrapping::NoWrap ? ScrollAxis::Both : ScrollAxis::Vertical,
      .children = {
        Text {
          .attributed = s,
          .wrapping = wrapping
        }
      }
    }.fill(FillStyle::solid(backgroundColor))
     .stroke(StrokeStyle::solid(borderColor, borderWidth))
     .cornerRadius(cornerRadius)
     .padding(padding)
     .focusable(!disabled)
     .cursor(Cursor::IBeam);
};

}