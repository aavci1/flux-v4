#include "Shell/ShellSystemStatus.hpp"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

namespace {

std::filesystem::path tempRoot(char const* name) {
  auto path = std::filesystem::temp_directory_path() /
              (std::string(name) + "-" + std::to_string(static_cast<unsigned long long>(getpid())));
  std::filesystem::remove_all(path);
  std::filesystem::create_directories(path);
  return path;
}

void writeFile(std::filesystem::path const& path, std::string const& text) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream(path) << text;
}

} // namespace

TEST_CASE("Shell system status reads docklet data without compositor snapshots") {
  auto root = tempRoot("lambda-shell-system-status-test");

  std::filesystem::create_directories(root / "class" / "net" / "wlan0" / "wireless");
  writeFile(root / "class" / "net" / "wlan0" / "operstate", "up\n");
  writeFile(root / "class" / "rfkill" / "rfkill0" / "type", "bluetooth\n");
  writeFile(root / "class" / "rfkill" / "rfkill0" / "state", "1\n");
  writeFile(root / "class" / "power_supply" / "BAT0" / "type", "Battery\n");
  writeFile(root / "class" / "power_supply" / "BAT0" / "capacity", "88\n");

  auto status = lambda_shell::readShellSystemStatus(root);
  CHECK(status.network == "online");
  CHECK(status.wifi == "wlan0");
  CHECK(status.bluetooth == "on");
  CHECK(status.volume == "unavailable");
  CHECK(status.battery == "88%");

  std::filesystem::remove_all(root);
}

TEST_CASE("Shell system status reports unavailable or off states from shell providers") {
  auto root = tempRoot("lambda-shell-system-status-off-test");

  std::filesystem::create_directories(root / "class" / "net" / "eth0");
  writeFile(root / "class" / "net" / "eth0" / "operstate", "down\n");
  writeFile(root / "class" / "rfkill" / "rfkill0" / "type", "bluetooth\n");
  writeFile(root / "class" / "rfkill" / "rfkill0" / "state", "0\n");

  auto status = lambda_shell::readShellSystemStatus(root);
  CHECK(status.network == "off");
  CHECK(status.wifi == "unavailable");
  CHECK(status.bluetooth == "off");
  CHECK(status.battery == "unavailable");

  std::filesystem::remove_all(root);
}
