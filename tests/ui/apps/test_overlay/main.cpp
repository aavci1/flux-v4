#include <Flux.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/Reactive/Reactive.hpp>
#include <Flux/UI/Overlay.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/Views/Button.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/VStack.hpp>
#include <Flux/UI/Views/ZStack.hpp>

#include <Flux/UI/Views/Rectangle.hpp>

using namespace flux;

struct OverlayTestRoot {
  auto body() const {
    auto [showDialog, hideDialog, isPresented] = useOverlay();
    (void)isPresented;
    return ZStack{
        .children =
            {
                Rectangle{.fill = FillStyle::solid(Color::hex(0xF2F2F7))},
                VStack{
                    .spacing = 16.f,
                    .padding = 40.f,
                    .hAlign = HorizontalAlignment::Leading,
                    .children =
                        {
                            Text{.text = "Overlay test",
                                 .font = {.size = 22.f, .weight = 700.f},
                                 .color = Color::hex(0x111118)},
                            Button{
                                .label = "Open",
                                .testFocusKey = "open-dialog",
                                .onTap =
                                    [showDialog, hideDialog] {
                                      showDialog(
                                          Element{VStack{
                                              .spacing = 12.f,
                                              .padding = 20.f,
                                              .hAlign = HorizontalAlignment::Leading,
                                              .children =
                                                  {
                                                      Text{.text = "Modal content",
                                                           .testFocusKey = "modal-title",
                                                           .font = {.size = 17.f, .weight = 600.f},
                                                           .color = Color::hex(0x111118)},
                                                      Button{.label = "Close",
                                                             .testFocusKey = "close-dialog",
                                                             .onTap = hideDialog},
                                                  },
                                          }},
                                          OverlayConfig{.modal = true,
                                                        .backdropColor = Color{0.f, 0.f, 0.f, 0.45f},
                                                        .dismissOnEscape = true,
                                                        .onDismiss = hideDialog});
                                    },
                            },
                        },
                },
            },
    };
  }
};

int main(int argc, char* argv[]) {
  Application app(argc, argv);
  auto& w = app.createWindow({
      .size = {480, 360},
      .title = "Flux UI test — overlay",
  });
  w.setView<OverlayTestRoot>();
  return app.exec();
}
