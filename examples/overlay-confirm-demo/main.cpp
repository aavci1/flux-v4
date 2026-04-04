#include <Flux.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/Reactive/Reactive.hpp>
#include <Flux/UI/Overlay.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/VStack.hpp>
#include <Flux/UI/Views/ZStack.hpp>

#include <functional>
#include <string>

using namespace flux;

namespace pal {
constexpr Color bg = Color::hex(0xF2F2F7);
constexpr Color cardStroke = Color::hex(0xE0E0E6);
constexpr Color titleC = Color::hex(0x111118);
constexpr Color bodyC = Color::hex(0x6E6E80);
constexpr Color dangerBg = Color::hex(0xFFECEC);
constexpr Color dangerStroke = Color::hex(0xF0AAAA);
constexpr Color dangerText = Color::hex(0xD94040);
} // namespace pal

struct ConfirmDialog {
  std::string title;
  std::string message;
  std::string confirmLabel = "Confirm";
  Color confirmColor = Color::hex(0xD94040);
  std::function<void()> onConfirm;
  std::function<void()> onCancel;

  auto body() const {
    Theme const& theme = useEnvironment<Theme>();
    return ZStack{
        .horizontalAlignment = Alignment::Center,
        .verticalAlignment = Alignment::Center,
        .children = children(
                VStack{
                    .spacing = 12.f,
                    .alignment = Alignment::Start,
                    .children = children(
                            ZStack{
                                .children = children(
                                        Rectangle{
                                            .fill = FillStyle::solid(Color::hex(0xFFFFFF)),
                                            .stroke = StrokeStyle::solid(pal::cardStroke, 1.f),
                                        }
                                            .width(360.f)
                                            .cornerRadius(CornerRadius(14.f)),
                                        VStack{
                                            .spacing = 12.f,
                                            .alignment = Alignment::Start,
                                            .children = children(
                                                    Text{.text = title,
                                                         .style = theme.typeTitle,
                                                         .color = pal::titleC},
                                                    HStack{
                                                        .spacing = 0.f,
                                                        .children = children(
                                                                Text{.text = message,
                                                                             .style = theme.typeBody,
                                                                             .color = pal::bodyC,
                                                                             .wrapping = TextWrapping::Wrap}
                                                                    .flex(1.f)
                                                            ),
                                                    },
                                                    HStack{
                                                        .spacing = 10.f,
                                                        .children = children(
                                                                ZStack{
                                                                    .children = children(
                                                                            Rectangle{
                                                                                .fill = FillStyle::solid(Color::hex(0xF0F0F5)),
                                                                                .stroke = StrokeStyle::solid(pal::cardStroke, 1.f),
                                                                            }
                                                                                .height(44.f)
                                                                                .cursor(Cursor::Hand)
                                                                                .focusable(true)
                                                                                .onKeyDown(
                                                                                    [cb = onCancel](KeyCode k, Modifiers) {
                                                                                      if (k == keys::Return || k == keys::Space) {
                                                                                        cb();
                                                                                      }
                                                                                    })
                                                                                .onTap(onCancel)
                                                                                .cornerRadius(CornerRadius(8.f)),
                                                                            Text{.text = "Cancel",
                                                                                 .style = theme.typeLabel,
                                                                                 .color = pal::titleC,
                                                                                 .horizontalAlignment = HorizontalAlignment::Center,
                                                                                 .verticalAlignment = VerticalAlignment::Center}
                                                                        ),
                                                                }.flex(1.f),
                                                                ZStack{
                                                                    .children = children(
                                                                            Rectangle{
                                                                                .fill = FillStyle::solid(confirmColor),
                                                                                .stroke = StrokeStyle::none(),
                                                                            }
                                                                                .height(44.f)
                                                                                .cursor(Cursor::Hand)
                                                                                .focusable(true)
                                                                                .onKeyDown(
                                                                                    [cb = onConfirm](KeyCode k, Modifiers) {
                                                                                      if (k == keys::Return || k == keys::Space) {
                                                                                        cb();
                                                                                      }
                                                                                    })
                                                                                .onTap(onConfirm)
                                                                                .cornerRadius(CornerRadius(8.f)),
                                                                            Text{.text = confirmLabel,
                                                                                 .style = theme.typeLabel,
                                                                                 .color = Color::hex(0xFFFFFF),
                                                                                 .horizontalAlignment = HorizontalAlignment::Center,
                                                                                 .verticalAlignment = VerticalAlignment::Center}
                                                                        ),
                                                                }.flex(1.f)
                                                            ),
                                                    }
                                                ),
                                        }
                                            .width(360.f)
                                            .padding(24.f)
                                    ),
                            }
                        ),
                }.padding(24.f)
            ),
    };
  }
};

struct FileManagerRow {
  std::string filename;
  std::function<void()> onDelete;

  auto body() const {
    Theme const& theme = useEnvironment<Theme>();
    auto [showDialog, hideDialog, isPresented] = useOverlay();
    (void)isPresented;
    std::string const fn = filename;
    std::function<void()> const del = onDelete;

    return HStack{
        .spacing = 12.f,
        .children = children(
                Text{.text = filename, .style = theme.typeBody, .color = pal::titleC}.flex(1.f),
                ZStack{
                    .children = children(
                            Rectangle{
                                .fill = FillStyle::solid(pal::dangerBg),
                                .stroke = StrokeStyle::solid(pal::dangerStroke, 1.f),
                            }
                                .size(88.f, 32.f)
                                .cursor(Cursor::Hand)
                                .onTap(
                                    [fn, del, showDialog, hideDialog] {
                                      showDialog(
                                          ConfirmDialog{
                                              .title = "Delete file?",
                                              .message = "\"" + fn + "\" will be permanently removed.",
                                              .confirmLabel = "Delete",
                                              .confirmColor = Color::hex(0xD94040),
                                              .onConfirm =
                                                  [del, hideDialog] {
                                                    del();
                                                    hideDialog();
                                                  },
                                              .onCancel = hideDialog,
                                          },
                                          OverlayConfig{
                                              .modal = true,
                                              .backdropColor = Color{0.f, 0.f, 0.f, 0.45f},
                                              .dismissOnOutsideTap = false,
                                              .dismissOnEscape = true,
                                              .onDismiss = hideDialog,
                                          });
                                    })
                                .cornerRadius(CornerRadius(6.f)),
                            Text{.text = "Delete",
                                 .style = theme.typeLabel,
                                 .color = pal::dangerText,
                                 .horizontalAlignment = HorizontalAlignment::Center,
                                 .verticalAlignment = VerticalAlignment::Center}
                        ),
                }
            ),
    }.padding(8.f);
  }
};

struct OverlayConfirmRoot {
  auto body() const {
    Theme const& theme = useEnvironment<Theme>();
    return ZStack{
        .children = children(
                Rectangle{.fill = FillStyle::solid(pal::bg)},
                VStack{
                    .spacing = 16.f,
                    .alignment = Alignment::Start,
                    .children = children(
                            Text{.text = "Overlay — confirm dialog",
                                 .style = theme.typeDisplay,
                                 .color = pal::titleC},
                            HStack{
                                .spacing = 0.f,
                                .children = children(
                                        Text{
                                                .text = "Tap Delete on a row. Modal overlay traps Tab between Cancel and Delete; "
                                                        "Escape or the buttons dismiss.",
                                                .style = theme.typeBody,
                                                .color = pal::bodyC,
                                                .wrapping = TextWrapping::Wrap,
                                            }
                                            .flex(1.f)
                                    ),
                            },
                            Element{FileManagerRow{.filename = "notes.txt", .onDelete = [] {}}}.flex(1.f),
                            Element{FileManagerRow{.filename = "draft.md", .onDelete = [] {}}}.flex(1.f)
                        ),
                }.padding(24.f)
            ),
    };
  }
};

int main(int argc, char* argv[]) {
  Application app(argc, argv);
  auto& w = app.createWindow<Window>({
      .size = {560, 480},
      .title = "Flux — Overlay confirm demo",
      .resizable = true,
  });
  w.setView<OverlayConfirmRoot>();
  return app.exec();
}
