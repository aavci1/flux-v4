#include "SettingsApp.hpp"

#include <Flux.hpp>
#include <Flux/UI/Application.hpp>
#include <Flux/UI/Window.hpp>

int main(int argc, char* argv[]) {
  flux::Application app(argc, argv);
  app.setName("settings");

  auto& window = app.createWindow<flux::Window>({
      .size = {780.f, 520.f},
      .title = "Settings",
      .decorationMode = flux::WindowDecorationMode::IntegratedTitlebar,
      .resizable = true,
      .glass = {
          .enabled = true,
      },
  });
  if (window.platformCapabilities().supportsWindowGlass) {
    window.setClearColor(flux::Colors::transparent);
  }
  window.setView<lambda_settings::SettingsAppRoot>({.window = &window});

  return app.exec();
}
