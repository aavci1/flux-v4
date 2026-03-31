#pragma once

#include <Flux/Core/Cursor.hpp>
#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/Font.hpp>
#include <Flux/Graphics/Styles.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/Theme.hpp>

#include <functional>
#include <string>

namespace flux {

struct TextAreaHeight {
  float fixed = 0.f;
  float minIntrinsic = 80.f;
  float maxIntrinsic = 0.f;
};

struct TextArea {
  State<std::string> value{};
  std::string placeholder;

  Font font = kFontFromTheme;

  Color textColor = kFromTheme;
  Color placeholderColor = kFromTheme;
  Color backgroundColor = kFromTheme;
  Color borderColor = kFromTheme;
  Color borderFocusColor = kFromTheme;
  Color caretColor = kFromTheme;
  Color selectionColor = kFromTheme;
  Color disabledColor = kFromTheme;

  float borderWidth = 1.f;
  float borderFocusWidth = 2.f;
  float cornerRadius = kFloatFromTheme;
  float paddingH = kFloatFromTheme;
  float paddingV = kFloatFromTheme;
  float lineHeight = 0.f;

  TextAreaHeight height{};

  float flexGrow = 1.f;
  float flexShrink = 0.f;
  float minSize = 0.f;

  bool disabled = false;
  int maxLength = 0;

  std::function<void(std::string const&)> onChange;
  std::function<void(std::string const&)> onEscape;

  Element body() const;
};

} // namespace flux
