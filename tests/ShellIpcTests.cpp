#include <Flux/Shell/ShellIpc.hpp>

#include <doctest/doctest.h>

TEST_CASE("shell IPC escape and roundtrip launchApp") {
  std::string const appId = R"(app"with\slashes)";
  std::string const line = flux::shell::serializeLaunchApp(appId);
  auto message = flux::shell::parseLine(line);
  REQUIRE(message.has_value());
  REQUIRE(message->kind == flux::shell::ShellMessageKind::WindowManagerLaunchApp);
  REQUIRE(message->launchApp.appId == appId);
}

TEST_CASE("shell IPC hello roundtrip") {
  std::string const line =
      flux::shell::serializeShellHello(1, "0.2.0", {"topbar", "dock", "command-launcher"});
  auto message = flux::shell::parseLine(line);
  REQUIRE(message.has_value());
  REQUIRE(message->kind == flux::shell::ShellMessageKind::ShellHello);
  REQUIRE(message->hello.protocolVersion == 1);
  REQUIRE(message->hello.shellVersion == "0.2.0");
  REQUIRE(message->hello.capabilities.size() == 3);
}

TEST_CASE("shell IPC malformed lines do not crash") {
  REQUIRE_FALSE(flux::shell::parseLine("").has_value());
  REQUIRE_FALSE(flux::shell::parseLine("{not json").has_value());
  REQUIRE_FALSE(flux::shell::parseLine(R"({"type":"unknown.message"})").has_value());

  auto partial = flux::shell::parseLine(R"({"type":"lambda.windowManager.launchApp"})");
  REQUIRE(partial.has_value());
  REQUIRE(partial->kind == flux::shell::ShellMessageKind::WindowManagerLaunchApp);
  REQUIRE(partial->launchApp.appId.empty());
}

TEST_CASE("shell IPC focusWindow parses numeric id") {
  auto message = flux::shell::parseLine(R"({"type":"lambda.windowManager.focusWindow","windowId":42})");
  REQUIRE(message.has_value());
  REQUIRE(message->kind == flux::shell::ShellMessageKind::WindowManagerFocusWindow);
  REQUIRE(message->focusWindow.windowId == 42u);
}

TEST_CASE("shell IPC serialize helpers match parseLine") {
  std::string const claim = flux::shell::serializeClaimCommandLauncherModal();
  auto claimMessage = flux::shell::parseLine(claim);
  REQUIRE(claimMessage.has_value());
  REQUIRE(claimMessage->kind == flux::shell::ShellMessageKind::WindowManagerClaimCommandLauncherModal);

  std::string const error = flux::shell::serializeWindowManagerError("not-found", "missing app");
  auto errorMessage = flux::shell::parseLine(error);
  REQUIRE(errorMessage.has_value());
  REQUIRE(errorMessage->kind == flux::shell::ShellMessageKind::WindowManagerError);
  REQUIRE(errorMessage->error.code == "not-found");
  REQUIRE(errorMessage->error.message == "missing app");
}
