// Demonstrates FluxTheme, useEnvironment, Element::environment(), and window-level theme switching.
#include <Flux.hpp>
#include <Flux/Core/Application.hpp>
#include <Flux/Core/Window.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Button.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/VStack.hpp>

using namespace flux;

Window* gThemeDemoWindow = nullptr;

struct ThemeDemoRoot {
  auto body() const {
    auto windowDark = useState(false);

    FluxTheme const& windowTheme = useEnvironment<FluxTheme>();

    auto pane = [](char const* title, char const* subtitle, FluxTheme const& t) {
      return VStack{
          .spacing = 8.f,
          .padding = 16.f,
          .hAlign = HorizontalAlignment::Leading,
          .children =
              {
                  Text{ .text = title,
                        .font = t.fontHeading,
                        .color = t.textPrimary },
                  Text{ .text = subtitle,
                        .font = t.fontBody,
                        .color = t.textSecondary,
                        .wrapping = TextWrapping::Wrap,
                        .frame = { 0.f, 0.f, 200.f, 0.f } },
                  Button{ .label = "Accent",
                          .variant = ButtonVariant::Primary,
                          .onTap = [] {} },
              },
      };
    };

    return ZStack{
        .children =
            {
                Rectangle{ .fill = FillStyle::solid(windowTheme.surfaceBackground) },
                VStack{
                    .spacing = 16.f,
                    .padding = 20.f,
                    .hAlign = HorizontalAlignment::Leading,
                    .children =
                        {
                            Text{ .text = "Theme & environment",
                                  .font = windowTheme.fontHeading,
                                  .color = windowTheme.textPrimary },
                            Text{
                                .text = "Window theme drives defaults. The right column uses "
                                        "Element::environment(FluxTheme::dark()) for a subtree override.",
                                .font = windowTheme.fontBody,
                                .color = windowTheme.textSecondary,
                                .wrapping = TextWrapping::Wrap,
                                .frame = { 0.f, 0.f, 520.f, 0.f },
                            },
                            Button{
                                .label = *windowDark ? "Switch window to light" : "Switch window to dark",
                                .variant = ButtonVariant::Secondary,
                                .onTap = [windowDark] {
                                  if (!gThemeDemoWindow) {
                                    return;
                                  }
                                  bool const next = !*windowDark;
                                  gThemeDemoWindow->setEnvironmentValue(next ? FluxTheme::dark()
                                                                           : FluxTheme::light());
                                  windowDark = next;
                                  Application::instance().markReactiveDirty();
                                },
                            },
                            HStack{
                                .spacing = 12.f,
                                .vAlign = VerticalAlignment::Top,
                                .children =
                                    {
                                        Element{ pane("Light pane",
                                                      "Subtree explicitly set to FluxTheme::light().",
                                                      FluxTheme::light()) }
                                            .environment(FluxTheme::light()),
                                        Element{ pane("Dark pane",
                                                      "Subtree uses FluxTheme::dark() tokens.",
                                                      FluxTheme::dark()) }
                                            .environment(FluxTheme::dark()),
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
  auto& w = app.createWindow<Window>({
      .size = {640, 480},
      .title = "Flux — Theme demo",
      .resizable = true,
  });
  gThemeDemoWindow = &w;
  w.setView<ThemeDemoRoot>();
  return app.exec();
}
