#include <Flux/UI/Views/TextInput.hpp>

#include <Flux/Core/KeyCodes.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/Reactive/Effect.hpp>
#include <Flux/SceneGraph/InteractionData.hpp>
#include <Flux/SceneGraph/RectNode.hpp>
#include <Flux/SceneGraph/TextNode.hpp>
#include <Flux/UI/InputFieldChrome.hpp>
#include <Flux/UI/InputFieldLayout.hpp>
#include <Flux/UI/MeasureContext.hpp>
#include <Flux/UI/MountContext.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Text.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

namespace flux {

namespace {

Theme const& activeTheme(EnvironmentStack& environment) {
  if (Theme const* theme = environment.find<Theme>()) {
    return *theme;
  }
  static Theme const fallback = Theme::light();
  return fallback;
}

struct ResolvedTextInputStyle {
  Font font{};
  Color textColor{};
  Color placeholderColor{};
  Color caretColor{};
  Color selectionColor{};
  ResolvedInputFieldChrome chrome{};
  float lineHeight = 0.f;
  float height = 0.f;
};

ResolvedTextInputStyle resolveTextInputStyle(TextInput::Style const& style,
                                             Theme const& theme) {
  InputFieldChromeSpec chromeSpec{};
  chromeSpec.backgroundColor = style.backgroundColor;
  chromeSpec.borderColor = style.borderColor;
  chromeSpec.borderFocusColor = style.borderFocusColor;
  chromeSpec.disabledColor = style.disabledColor;
  chromeSpec.cornerRadius = style.cornerRadius;
  chromeSpec.borderWidth = style.borderWidth;
  chromeSpec.borderFocusWidth = style.borderFocusWidth;
  chromeSpec.paddingH = style.paddingH;
  chromeSpec.paddingV = style.paddingV;

  ResolvedInputFieldChrome chrome = resolveInputFieldChrome(chromeSpec, theme);
  return ResolvedTextInputStyle{
      .font = resolveFont(style.font, theme.bodyFont, theme),
      .textColor = resolveColor(style.textColor, theme.labelColor, theme),
      .placeholderColor = resolveColor(style.placeholderColor, theme.placeholderTextColor, theme),
      .caretColor = resolveColor(style.caretColor, theme.accentColor, theme),
      .selectionColor = resolveColor(style.selectionColor, theme.accentColor, theme),
      .chrome = chrome,
      .lineHeight = std::max(0.f, style.lineHeight),
      .height = std::max(0.f, style.height),
  };
}

TextLayoutOptions textInputLayoutOptions(bool multiline) {
  TextLayoutOptions options{};
  options.wrapping = multiline ? TextWrapping::Wrap : TextWrapping::NoWrap;
  options.horizontalAlignment = HorizontalAlignment::Leading;
  options.verticalAlignment = VerticalAlignment::Top;
  return options;
}

AttributedString attributedText(TextInput const& input,
                                ResolvedTextInputStyle const& style) {
  std::string const text = input.value.get();
  if (text.empty()) {
    return AttributedString::plain(input.placeholder, style.font, style.placeholderColor);
  }

  AttributedString attributed;
  attributed.utf8 = text;
  if (input.styler) {
    attributed.runs = input.styler(attributed.utf8);
  }
  if (attributed.runs.empty()) {
    attributed.runs.push_back(AttributedRun{
        .start = 0,
        .end = static_cast<std::uint32_t>(attributed.utf8.size()),
        .font = style.font,
        .color = style.textColor,
    });
  }
  return attributed;
}

Size textInputFrameSize(TextInput const& input, ResolvedTextInputStyle const& style,
                        LayoutConstraints const& constraints, TextSystem& textSystem) {
  float const borderInset = std::max(style.chrome.borderWidth, style.chrome.borderFocusWidth);
  float const padX = style.chrome.paddingH + borderInset;
  float const padY = style.chrome.paddingV + borderInset;
  float maxTextWidth = std::isfinite(constraints.maxWidth)
                           ? std::max(0.f, constraints.maxWidth - 2.f * padX)
                           : 0.f;
  if (!input.multiline) {
    maxTextWidth = 0.f;
  }

  TextLayoutOptions const options = textInputLayoutOptions(input.multiline);
  Size measured = textSystem.measure(attributedText(input, style), maxTextWidth, options);
  measured.width += 2.f * padX;
  measured.height += 2.f * padY;

  if (input.multiline) {
    if (input.multilineHeight.fixed > 0.f) {
      measured.height = input.multilineHeight.fixed;
    } else {
      measured.height = std::max(measured.height, input.multilineHeight.minIntrinsic);
      if (input.multilineHeight.maxIntrinsic > 0.f) {
        measured.height = std::min(measured.height, input.multilineHeight.maxIntrinsic);
      }
    }
  } else {
    measured.height = resolvedInputFieldHeight(style.font, style.textColor,
                                               style.chrome.paddingV + borderInset,
                                               style.height);
  }

  if (std::isfinite(constraints.maxWidth)) {
    measured.width = std::min(measured.width, constraints.maxWidth);
  }
  if (std::isfinite(constraints.maxHeight)) {
    measured.height = std::min(measured.height, constraints.maxHeight);
  }
  measured.width = std::max(measured.width, constraints.minWidth);
  measured.height = std::max(measured.height, constraints.minHeight);
  return measured;
}

Rect textBox(Size frameSize, ResolvedTextInputStyle const& style) {
  float const borderInset = std::max(style.chrome.borderWidth, style.chrome.borderFocusWidth);
  float const padX = style.chrome.paddingH + borderInset;
  float const padY = style.chrome.paddingV + borderInset;
  return Rect{padX, padY, std::max(0.f, frameSize.width - 2.f * padX),
              std::max(0.f, frameSize.height - 2.f * padY)};
}

void setTextLayout(scenegraph::TextNode& node, TextInput const& input,
                   ResolvedTextInputStyle const& style, TextSystem& textSystem,
                   Size frameSize) {
  Rect const box = textBox(frameSize, style);
  node.setBounds(box);
  node.setLayout(textSystem.layout(attributedText(input, style), box,
                                   textInputLayoutOptions(input.multiline)));
}

std::string appendLimited(std::string value, std::string const& text, int maxLength) {
  value += text;
  if (maxLength > 0 && static_cast<int>(value.size()) > maxLength) {
    value.resize(static_cast<std::size_t>(maxLength));
  }
  return value;
}

} // namespace

Size TextInput::measure(MeasureContext& ctx, LayoutConstraints const& constraints,
                        LayoutHints const&, TextSystem& textSystem) const {
  ctx.advanceChildSlot();
  Theme const& theme = useEnvironment<Theme>();
  return textInputFrameSize(*this, resolveTextInputStyle(style, theme), constraints, textSystem);
}

std::unique_ptr<scenegraph::SceneNode> TextInput::mount(MountContext& ctx) const {
  Theme const& theme = activeTheme(ctx.environment());
  ResolvedTextInputStyle const resolved = resolveTextInputStyle(style, theme);
  Size const frameSize = textInputFrameSize(*this, resolved, ctx.constraints(), ctx.textSystem());

  auto wrapper = std::make_unique<scenegraph::RectNode>(
      Rect{0.f, 0.f, frameSize.width, frameSize.height},
      FillStyle::solid(disabled ? resolved.chrome.disabledColor
                                : resolved.chrome.backgroundColor),
      StrokeStyle::solid(resolved.chrome.borderColor, resolved.chrome.borderWidth),
      CornerRadius{resolved.chrome.cornerRadius});

  auto textNode = std::make_unique<scenegraph::TextNode>();
  scenegraph::TextNode* rawText = textNode.get();
  setTextLayout(*rawText, *this, resolved, ctx.textSystem(), frameSize);
  wrapper->appendChild(std::move(textNode));

  auto interaction = std::make_unique<scenegraph::InteractionData>();
  interaction->cursor = disabled ? Cursor::Inherit : Cursor::IBeam;
  interaction->focusable = !disabled;
  State<std::string> valueState = value;
  int const lengthLimit = maxLength;
  bool const acceptsMultiline = multiline;
  auto onChangeHandler = onChange;
  auto onSubmitHandler = onSubmit;
  auto onEscapeHandler = onEscape;
  bool const isDisabled = disabled;
  interaction->onTextInput = [valueState, lengthLimit, onChangeHandler, isDisabled](std::string const& text) {
    if (isDisabled || text.empty()) {
      return;
    }
    std::string next = appendLimited(valueState.get(), text, lengthLimit);
    valueState = next;
    if (onChangeHandler) {
      onChangeHandler(next);
    }
  };
  interaction->onKeyDown = [valueState, acceptsMultiline, lengthLimit, onChangeHandler,
                            onSubmitHandler, onEscapeHandler, isDisabled](KeyCode key, Modifiers) {
    if (isDisabled) {
      return;
    }
    if (key == keys::Delete) {
      std::string next = valueState.get();
      if (!next.empty()) {
        next.pop_back();
        valueState = next;
        if (onChangeHandler) {
          onChangeHandler(next);
        }
      }
      return;
    }
    if (key == keys::Return) {
      if (acceptsMultiline) {
        std::string next = appendLimited(valueState.get(), "\n", lengthLimit);
        valueState = next;
        if (onChangeHandler) {
          onChangeHandler(next);
        }
      } else if (onSubmitHandler) {
        onSubmitHandler(valueState.get());
      }
      return;
    }
    if (key == keys::Escape && onEscapeHandler) {
      onEscapeHandler(valueState.get());
    }
  };
  wrapper->setInteraction(std::move(interaction));

  Reactive::withOwner(ctx.owner(), [rawText, input = *this, resolved, frameSize,
                                    textSystem = &ctx.textSystem(),
                                    requestRedraw = ctx.redrawCallback()] {
    Reactive::Effect([rawText, input, resolved, frameSize, textSystem, requestRedraw] {
      (void)input.value.get();
      setTextLayout(*rawText, input, resolved, *textSystem, frameSize);
      if (requestRedraw) {
        requestRedraw();
      }
    });
  });

  return wrapper;
}

Element TextInput::body() const {
  return Text{
      .text = value.get().empty() ? placeholder : value.get(),
      .font = style.font,
      .color = style.textColor,
      .wrapping = multiline ? TextWrapping::Wrap : TextWrapping::NoWrap,
  };
}

} // namespace flux
