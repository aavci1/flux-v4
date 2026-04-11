#include <Flux/UI/Views/TextArea.hpp>

#include <Flux/UI/Views/TextInput.hpp>

namespace flux {

namespace {

TextInput::Style asTextInputStyle(TextArea::Style const& style) {
    return TextInput::Style {
        .font = style.font,
        .textColor = style.textColor,
        .placeholderColor = style.placeholderColor,
        .backgroundColor = style.backgroundColor,
        .borderColor = style.borderColor,
        .borderFocusColor = style.borderFocusColor,
        .caretColor = style.caretColor,
        .selectionColor = style.selectionColor,
        .disabledColor = style.disabledColor,
        .borderWidth = style.borderWidth,
        .borderFocusWidth = style.borderFocusWidth,
        .cornerRadius = style.cornerRadius,
        .paddingH = style.paddingH,
        .paddingV = style.paddingV,
        .lineHeight = style.lineHeight,
    };
}

} // namespace

Element TextArea::body() const {
    return TextInput {
        .value = value,
        .placeholder = placeholder,
        .styler = styler,
        .style = asTextInputStyle(style),
        .multiline = true,
        .disabled = disabled,
        .maxLength = maxLength,
        .multilineHeight =
            TextInputHeight {
                .fixed = height.fixed,
                .minIntrinsic = height.minIntrinsic,
                .maxIntrinsic = height.maxIntrinsic,
            },
        .onChange = onChange,
        .onEscape = onEscape,
    };
}

} // namespace flux
