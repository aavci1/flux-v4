#include <Flux/Core/Window.hpp>
#include <Flux/Detail/Runtime.hpp>
#include <Flux/UI/StateStore.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Alert.hpp>
#include <Flux/UI/Overlay.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/Spacer.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/VStack.hpp>
#include <Flux/UI/Views/ZStack.hpp>

#include <utility>

namespace flux {

Element Alert::body() const {
  FluxTheme const& theme = useEnvironment<FluxTheme>();
  Color const card = resolveColor(cardColor, theme.colorSurface);
  Color const stroke = resolveColor(cardStrokeColor, theme.colorBorderSubtle);
  Color const titleC = resolveColor(titleColor, theme.colorTextPrimary);
  Color const msgC = resolveColor(messageColor, theme.colorTextSecondary);
  CornerRadius const cardCorner{resolveFloat(cornerRadius, theme.radiusXLarge)};
  // A lone ZStack under the full-window overlay expands every child to the window size
  // (see LayoutZStack — children share the stack's max proposed size). Spacers + flex center
  // the card so the inner ZStack only receives the card's intrinsic width/height.
  return VStack{
      .spacing = 0.f,
      .hAlign = HorizontalAlignment::Center,
      .children =
          {
              Element{Spacer{}}.withFlex(1.f),
              HStack{
                  .spacing = 0.f,
                  .children =
                      {
                          Element{Spacer{}}.withFlex(1.f),
                          ZStack{
                              // Card + content share top-left; center alignment would offset each
                              // child by its own measured size and misalign the background.
                              .hAlign = HorizontalAlignment::Leading,
                              .vAlign = VerticalAlignment::Top,
                              .children =
                                  {
                                      Rectangle{
                                          .frame = {0.f, 0.f, cardWidth, 0.f},
                                          .cornerRadius = cardCorner,
                                          .fill = FillStyle::solid(card),
                                          .stroke = StrokeStyle::solid(stroke, 1.f),
                                      },
                                      VStack{
                                          .spacing = theme.space3,
                                          .padding = theme.space6,
                                          .hAlign = HorizontalAlignment::Leading,
                                          .children = buildContent(titleC, msgC, theme),
                                      },
                                  },
                          },
                          Element{Spacer{}}.withFlex(1.f),
                      },
              },
              Element{Spacer{}}.withFlex(1.f),
          },
  };
}

std::vector<Element> Alert::buildContent(Color titleC, Color msgC, FluxTheme const& theme) const {
  std::vector<Element> rows;

  float const contentW = std::max(0.f, cardWidth - 2.f * theme.space6);
  rows.push_back(Text{
      .text = title,
      .font = theme.typeTitle.toFont(),
      .color = titleC,
      .lineHeight = theme.typeTitle.lineHeight,
      .frame = {0.f, 0.f, contentW, 0.f},
  });

  if (!message.empty()) {
    rows.push_back(Text{
        .text = message,
        .font = theme.typeBody.toFont(),
        .color = msgC,
        .wrapping = TextWrapping::Wrap,
        .lineHeight = theme.typeBody.lineHeight,
        .frame = {0.f, 0.f, contentW, 0.f},
    });
  }

  if (buttons.size() == 1) {
    auto const& btn = buttons[0];
    rows.push_back(HStack{
        .spacing = theme.space2,
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
        .spacing = theme.space2,
        .children = std::move(buttonElems),
    });
  }

  return rows;
}

std::tuple<std::function<void(Alert)>, std::function<void()>, bool> useAlert() {
  auto [showOverlay, hideOverlay, isPresented] = useOverlay();
  StateStore* store = StateStore::current();
  Runtime* rt = Runtime::current();
  assert(store && rt && "useAlert must be called inside body()");
  (void)store;
  Window* wPtr = &rt->window();

  auto show = [showOverlay, hideOverlay, wPtr](Alert alert) {
    if (alert.buttons.empty()) {
      // Empty action (not hideOverlay): the loop below wraps every button with
      // hideOverlay() then originalAction. OK only needs dismiss — same result as
      // spec's hideOverlay on the slot, without stuffing hideOverlay into original
      // and running it twice through the wrapper.
      alert.buttons.push_back({
          .label = "OK",
          .variant = ButtonVariant::Secondary,
          .action = {},
      });
    }

    // Cap at three buttons in all builds (assert would vanish in release).
    if (alert.buttons.size() > 3) {
      alert.buttons.resize(3);
    }

    for (auto& btn : alert.buttons) {
      auto originalAction = std::move(btn.action);
      btn.action = [hideOverlay, originalAction = std::move(originalAction)]() {
        hideOverlay();
        if (originalAction) {
          originalAction();
        }
      };
    }

    // show() runs outside a build pass — read window storage, not useEnvironment (backdrop is show-time).
    FluxTheme const* tp = wPtr->environmentValue<FluxTheme>();
    FluxTheme const theme = tp ? *tp : FluxTheme::light();
    Color const backdrop = resolveColor(alert.backdropColor, theme.colorScrimModal);
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
