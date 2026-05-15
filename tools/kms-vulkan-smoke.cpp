#include <Flux.hpp>

#include <chrono>
#include <cstdio>
#include <exception>
#include <thread>

int main(int argc, char** argv) {
  try {
    flux::Application app(argc, argv);
    flux::WindowConfig config{};
    config.title = "Flux KMS Vulkan Smoke";
    config.size = {640.f, 480.f};
    config.fullscreen = true;

    flux::Window& window = app.createWindow<flux::Window>(config);
    flux::Canvas& canvas = window.canvas();
    canvas.beginFrame();
    canvas.clear(flux::Color{0.f, 0.45f, 1.f, 1.f});
    canvas.present();

    std::puts("Flux KMS Vulkan smoke: presented blue frame");
    std::this_thread::sleep_for(std::chrono::milliseconds(750));
    return 0;
  } catch (std::exception const& e) {
    std::fprintf(stderr, "Flux KMS Vulkan smoke failed: %s\n", e.what());
    return 1;
  }
}
