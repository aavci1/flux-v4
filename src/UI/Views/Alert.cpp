#include <Flux/UI/Views/Alert.hpp>
#include <Flux/UI/Overlay.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/Spacer.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/VStack.hpp>
#include <Flux/UI/Views/ZStack.hpp>

#include <cassert>
#include <utility>

namespace flux {

Element Alert::body() const {
  return ZStack{
      .hAlign = HorizontalAlignment::Center,
      .vAlign = VerticalAlignment::Center,
      .children =
          {
              ZStack{
                  .children =
                      {
                          Rectangle{
                              .frame = {0.f, 0.f, cardWidth, 0.f},
                              .cornerRadius = cornerRadius,
                              .fill = FillStyle::solid(cardColor),
                              .stroke = StrokeStyle::solid(cardStrokeColor, 1.f),
                          },
                          VStack{
                              .spacing = 12.f,
                              .padding = 24.f,
                              .hAlign = HorizontalAlignment::Leading,
                              .children = buildContent(),
                          },
                      },
              },
          },
  };
}

std::vector<Element> Alert::buildContent() const {
  std::vector<Element> rows;

  rows.push_back(Text{
      .text = title,
      .font = {.size = 17.f, .weight = 600.f},
      .color = titleColor,
  });

  if (!message.empty()) {
    rows.push_back(Text{
        .text = message,
        .font = {.size = 14.f, .weight = 400.f},
        .color = messageColor,
        .wrapping = TextWrapping::Wrap,
        .frame = {0.f, 0.f, cardWidth - 48.f, 0.f},
    });
  }

  if (buttons.size() == 1) {
    auto const& btn = buttons[0];
    rows.push_back(HStack{
        .spacing = 8.f,
        .children =
            {
                Spacer{},
                Element{Button{
                    .label = btn.label,
                    .variant = btn.variant,
                    .disabled = btn.disabled,
                    .onTap = btn.action,
                }},
            },
    });
  } else {
    std::vector<Element> buttonElems;
    buttonElems.reserve(buttons.size());
    for (auto const& btn : buttons) {
      buttonElems.push_back(
          Element{Button{
                      .label = btn.label,
                      .variant = btn.variant,
                      .disabled = btn.disabled,
                      .onTap = btn.action,
                  }}
              .withFlex(1.f));
    }
    rows.push_back(HStack{
        .spacing = 8.f,
        .children = std::move(buttonElems),
    });
  }

  return rows;
}

std::tuple<std::function<void(Alert)>, std::function<void()>, bool> useAlert() {
  auto [showOverlay, hideOverlay, isPresented] = useOverlay();

  auto show = [showOverlay, hideOverlay](Alert alert) {
    if (alert.buttons.empty()) {
      alert.buttons.push_back({
          .label = "OK",
          .variant = ButtonVariant::Secondary,
          .action = {},
      });
    }

    assert(alert.buttons.size() <= 3 && "Alert supports at most three buttons");

    for (auto& btn : alert.buttons) {
      auto originalAction = std::move(btn.action);
      btn.action = [hideOverlay, originalAction = std::move(originalAction)]() {
        hideOverlay();
        if (originalAction) {
          originalAction();
        }
      };
    }

    Color const backdrop = alert.backdropColor;
    bool const dismissEsc = alert.dismissOnEscape;

    showOverlay(
        Element{std::move(alert)},
        OverlayConfig{
            .modal = true,
            .backdropColor = backdrop,
            .dismissOnOutsideTap = false,
            .dismissOnEscape = dismissEsc,
            .onDismiss = hideOverlay,
        });
  };

  return {std::move(show), hideOverlay, isPresented};
}

} // namespace flux
