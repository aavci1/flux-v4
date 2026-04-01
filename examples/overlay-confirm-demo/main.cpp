#include <Flux.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/Reactive/Reactive.hpp>
#include <Flux/UI/Overlay.hpp>
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
    return ZStack{
        .hAlign = HorizontalAlignment::Center,
        .vAlign = VerticalAlignment::Center,
        .children =
            {
                VStack{
                    .spacing = 12.f,
                    .padding = 24.f,
                    .hAlign = HorizontalAlignment::Leading,
                    .children =
                        {
                            ZStack{
                                .children =
                                    {
                                        Rectangle{
                                            .offsetX = 0.f, .offsetY = 0.f, .width = 360.f, .height = 0.f,
                                            .cornerRadius = CornerRadius(14.f),
                                            .fill = FillStyle::solid(Color::hex(0xFFFFFF)),
                                            .stroke = StrokeStyle::solid(pal::cardStroke, 1.f),
                                        },
                                        VStack{
                                            .spacing = 12.f,
                                            .padding = 24.f,
                                            .hAlign = HorizontalAlignment::Leading,
                                            .children =
                                                {
                                                    Text{.text = title,
                                                         .font = {.size = 17.f, .weight = 600.f},
                                                         .color = pal::titleC},
                                                    HStack{
                                                        .spacing = 0.f,
                                                        .children =
                                                            {
                                                                Element{Text{.text = message,
                                                                             .font = {.size = 14.f, .weight = 400.f},
                                                                             .color = pal::bodyC,
                                                                             .wrapping = TextWrapping::Wrap}}
                                                                    .withFlex(1.f),
                                                            },
                                                    },
                                                    HStack{
                                                        .spacing = 10.f,
                                                        .children =
                                                            {
                                                                Element{ZStack{
                                                                    .children =
                                                                        {
                                                                            Rectangle{
                                                                                .offsetX = 0.f, .offsetY = 0.f, .width = 0.f, .height = 44.f,
                                                                                .cornerRadius = CornerRadius(8.f),
                                                                                .fill = FillStyle::solid(Color::hex(0xF0F0F5)),
                                                                                .stroke = StrokeStyle::solid(pal::cardStroke, 1.f),
                                                                                .cursor = Cursor::Hand,
                                                                                .focusable = true,
                                                                                .onKeyDown =
                                                                                    [cb = onCancel](KeyCode k, Modifiers) {
                                                                                      if (k == keys::Return || k == keys::Space) {
                                                                                        cb();
                                                                                      }
                                                                                    },
                                                                                .onTap = onCancel,
                                                                            },
                                                                            Text{.text = "Cancel",
                                                                                 .font = {.size = 14.f, .weight = 500.f},
                                                                                 .color = pal::titleC,
                                                                                 .horizontalAlignment = HorizontalAlignment::Center,
                                                                                 .verticalAlignment = VerticalAlignment::Center},
                                                                        },
                                                                }}.withFlex(1.f),
                                                                Element{ZStack{
                                                                    .children =
                                                                        {
                                                                            Rectangle{
                                                                                .offsetX = 0.f, .offsetY = 0.f, .width = 0.f, .height = 44.f,
                                                                                .cornerRadius = CornerRadius(8.f),
                                                                                .fill = FillStyle::solid(confirmColor),
                                                                                .stroke = StrokeStyle::none(),
                                                                                .cursor = Cursor::Hand,
                                                                                .focusable = true,
                                                                                .onKeyDown =
                                                                                    [cb = onConfirm](KeyCode k, Modifiers) {
                                                                                      if (k == keys::Return || k == keys::Space) {
                                                                                        cb();
                                                                                      }
                                                                                    },
                                                                                .onTap = onConfirm,
                                                                            },
                                                                            Text{.text = confirmLabel,
                                                                                 .font = {.size = 14.f, .weight = 600.f},
                                                                                 .color = Color::hex(0xFFFFFF),
                                                                                 .horizontalAlignment = HorizontalAlignment::Center,
                                                                                 .verticalAlignment = VerticalAlignment::Center},
                                                                        },
                                                                }}.withFlex(1.f),
                                                            },
                                                    },
                                                },
                                        },
                                    },
                            },
                        },
                },
            },
    };
  }
};

struct FileManagerRow {
  std::string filename;
  std::function<void()> onDelete;

  auto body() const {
    auto [showDialog, hideDialog, isPresented] = useOverlay();
    (void)isPresented;
    std::string const fn = filename;
    std::function<void()> const del = onDelete;

    return HStack{
        .spacing = 12.f,
        .padding = 8.f,
        .children =
            {
                Text{.text = filename, .font = {.size = 15.f, .weight = 400.f}, .color = pal::titleC, .flexGrow = 1.f},
                ZStack{
                    .children =
                        {
                            Rectangle{
                                .offsetX = 0.f, .offsetY = 0.f, .width = 88.f, .height = 32.f,
                                .cornerRadius = CornerRadius(6.f),
                                .fill = FillStyle::solid(pal::dangerBg),
                                .stroke = StrokeStyle::solid(pal::dangerStroke, 1.f),
                                .cursor = Cursor::Hand,
                                .onTap =
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
                                    },
                            },
                            Text{.text = "Delete",
                                 .font = {.size = 13.f, .weight = 500.f},
                                 .color = pal::dangerText,
                                 .horizontalAlignment = HorizontalAlignment::Center,
                                 .verticalAlignment = VerticalAlignment::Center},
                        },
                },
            },
    };
  }
};

struct OverlayConfirmRoot {
  auto body() const {
    return ZStack{
        .children =
            {
                Rectangle{.fill = FillStyle::solid(pal::bg)},
                VStack{
                    .spacing = 16.f,
                    .padding = 24.f,
                    .hAlign = HorizontalAlignment::Leading,
                    .children =
                        {
                            Text{.text = "Overlay — confirm dialog",
                                 .font = {.size = 22.f, .weight = 700.f},
                                 .color = pal::titleC},
                            HStack{
                                .spacing = 0.f,
                                .children =
                                    {
                                        Element{Text{
                                                .text = "Tap Delete on a row. Modal overlay traps Tab between Cancel and Delete; "
                                                        "Escape or the buttons dismiss.",
                                                .font = {.size = 14.f, .weight = 400.f},
                                                .color = pal::bodyC,
                                                .wrapping = TextWrapping::Wrap,
                                            }}
                                            .withFlex(1.f),
                                    },
                            },
                            Element{FileManagerRow{.filename = "notes.txt", .onDelete = [] {}}}.withFlex(1.f),
                            Element{FileManagerRow{.filename = "draft.md", .onDelete = [] {}}}.withFlex(1.f),
                        },
                },
            },
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
