#include "Compositor/Config/CompositorConfig.hpp"

#include <Flux/Graphics/ImageFillMode.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

namespace {

struct ScopedEnv {
  explicit ScopedEnv(char const* name)
      : name(name) {
    if (char const* value = std::getenv(name); value) {
      hadOriginal = true;
      original = value;
    }
  }

  ~ScopedEnv() {
    if (!hadOriginal) {
      unsetenv(name);
    } else {
      setenv(name, original.c_str(), 1);
    }
  }

  char const* name;
  bool hadOriginal = false;
  std::string original;
};

std::filesystem::path tempConfigPath() {
  auto path = std::filesystem::temp_directory_path() /
              ("flux-compositor-config-test-" + std::to_string(static_cast<unsigned long long>(getpid())) + ".toml");
  std::filesystem::remove(path);
  return path;
}

} // namespace

TEST_CASE("compositor config creates a default file when missing") {
  ScopedEnv configEnv("FLUX_COMPOSITOR_CONFIG");
  auto const path = std::filesystem::temp_directory_path() /
                    ("flux-compositor-config-test-dir-" +
                     std::to_string(static_cast<unsigned long long>(getpid()))) /
                    "config.toml";
  std::filesystem::remove_all(path.parent_path());
  setenv("FLUX_COMPOSITOR_CONFIG", path.c_str(), 1);

  auto loaded = flux::compositor::loadConfigWithMetadata();

  CHECK(loaded.path == path.string());
  CHECK(loaded.hasModifiedAt);
  CHECK(std::filesystem::exists(path));
  CHECK(loaded.config.scale == doctest::Approx(2.0f));
  CHECK(loaded.config.backgroundColor.r == doctest::Approx(51.f / 255.f));
  CHECK(loaded.config.backgroundColor.g == doctest::Approx(128.f / 255.f));
  CHECK(loaded.config.backgroundColor.b == doctest::Approx(242.f / 255.f));

  std::filesystem::remove_all(path.parent_path());
}

TEST_CASE("compositor config parses colors, wallpaper, and keybindings") {
  ScopedEnv configEnv("FLUX_COMPOSITOR_CONFIG");
  auto const path = tempConfigPath();
  std::ofstream file(path);
  file << "background = \"#112233\"\n";
  file << "wallpaper = \"/tmp/wallpaper.png\"\n";
  file << "wallpaper_mode = \"contain\"\n";
  file << "scale = 1.5\n";
  file << "animations = false\n";
  file << "hardware_cursor = false\n";
  file << "[keybindings]\n";
  file << "snap_left = \"ctrl+alt+left\"\n";
  file.close();
  setenv("FLUX_COMPOSITOR_CONFIG", path.c_str(), 1);

  auto loaded = flux::compositor::loadConfigWithMetadata();
  auto const& config = loaded.config;
  CHECK(loaded.path == path.string());
  CHECK(loaded.hasModifiedAt);
  CHECK(config.backgroundColor.r == doctest::Approx(17.f / 255.f));
  CHECK(config.backgroundColor.g == doctest::Approx(34.f / 255.f));
  CHECK(config.backgroundColor.b == doctest::Approx(51.f / 255.f));
  CHECK(config.wallpaperPath == "/tmp/wallpaper.png");
  CHECK(config.wallpaperMode == flux::ImageFillMode::Fit);
  CHECK(config.scale == doctest::Approx(1.5f));
  CHECK_FALSE(config.animationsEnabled);
  CHECK_FALSE(config.hardwareCursorEnabled);

  auto snapLeft = std::find_if(config.shortcutBindings.begin(), config.shortcutBindings.end(), [](auto const& binding) {
    return binding.action == flux::compositor::WaylandServer::ShortcutAction::SnapLeft;
  });
  REQUIRE(snapLeft != config.shortcutBindings.end());
  CHECK(snapLeft->ctrl);
  CHECK(snapLeft->alt);
  CHECK_FALSE(snapLeft->meta);

  std::filesystem::remove(path);
}

TEST_CASE("compositor config reports file changes") {
  ScopedEnv configEnv("FLUX_COMPOSITOR_CONFIG");
  auto const path = tempConfigPath();
  {
    std::ofstream file(path);
    file << "background = \"#223344\"\n";
  }
  setenv("FLUX_COMPOSITOR_CONFIG", path.c_str(), 1);

  auto loaded = flux::compositor::loadConfigWithMetadata();
  CHECK_FALSE(flux::compositor::configChanged(loaded));
  std::filesystem::remove(path);
  CHECK(flux::compositor::configChanged(loaded));
}
