#include <Flux/Core/Window.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/Detail/Runtime.hpp>
#include <Flux/UI/StateStore.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/OverlaySurfaceHelpers.hpp>
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
  Theme const& theme = useEnvironment<Theme>();
  ResolvedAlertCardColors const surface = resolveAlertCardColors(cardColor, cardStrokeColor, cornerRadius, theme);
  Color const card = surface.cardFill;
  Color const stroke = surface.cardStroke;
  Color const titleC = resolveColor(titleColor, theme.colorTextPrimary);
  Color const msgC = resolveColor(messageColor, theme.colorTextSecondary);
  CornerRadius const cardCorner = surface.cornerRadius;
  // A lone ZStack under the full-window overlay expands every child to the window size
  // (see LayoutZStack — children share the stack's max proposed size). Spacers + flex center
  // the card so the inner ZStack only receives the card's intrinsic width/height.
  ZStack const cardStack{
      // Card + content share top-left; center alignment would offset each
      // child by its own measured size and misalign the background.
      .horizontalAlignment = Alignment::Start,
      .verticalAlignment = Alignment::Start,
      .children = flux::children(
          Rectangle{}
              .fill(FillStyle::solid(card))
              .stroke(StrokeStyle::solid(stroke, 1.f))
              .size(cardWidth, 0.f)
              .cornerRadius(cardCorner),
          VStack{
              .spacing = theme.space3,
              .alignment = Alignment::Start,
              .children = buildContent(titleC, msgC, theme),
          }.padding(theme.space6)),
  };

  HStack const row{
      .spacing = 0.f,
      .children = flux::children(Spacer{}.flex(1.f), Element{cardStack}, Spacer{}.flex(1.f)),
  };

  return VStack{
      .spacing = 0.f,
      .alignment = Alignment::Center,
      .children = flux::children(Spacer{}.flex(1.f), Element{row}, Spacer{}.flex(1.f)),
  };
}

std::vector<Element> Alert::buildContent(Color titleC, Color msgC, Theme const& theme) const {
  std::vector<Element> rows;

  float const contentW = std::max(0.f, cardWidth - 2.f * theme.space6);
  rows.push_back(Text{
                     .text = title,
                     .font = theme.fontTitle,
                     .color = titleC,
                 }
                     .size(contentW, 0.f));

  if (!message.empty()) {
    rows.push_back(Text{
                       .text = message,
                       .font = theme.fontBody,
                       .color = msgC,
                       .wrapping = TextWrapping::Wrap,
                   }
                       .size(contentW, 0.f));
  }

  if (buttons.size() == 1) {
    auto const& btn = buttons[0];
    rows.push_back(HStack{
        .spacing = theme.space2,
        .children = flux::children(
                Spacer{},
                Button{
                    .label = btn.label,
                    .variant = btn.variant,
                    .disabled = btn.disabled,
                    .onTap = btn.action,
                }),
    });
  } else {
    std::vector<Element> buttonElems;
    buttonElems.reserve(buttons.size());
    for (auto const& btn : buttons) {
      buttonElems.push_back(
          Button{
              .label = btn.label,
              .variant = btn.variant,
              .disabled = btn.disabled,
              .onTap = btn.action,
          }
              .flex(1.f));
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
    Theme const* tp = wPtr->environmentValue<Theme>();
    Theme const theme = tp ? *tp : Theme::light();
    Color const backdrop = resolveAlertBackdropColor(alert.backdropColor, theme);
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
