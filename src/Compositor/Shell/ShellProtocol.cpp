#include "Compositor/Wayland/WaylandServerImpl.hpp"

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>

namespace flux::compositor {
namespace {

std::string runtimePath(char const* name) {
  if (char const* runtimeDir = std::getenv("XDG_RUNTIME_DIR"); runtimeDir && *runtimeDir) {
    return std::string(runtimeDir) + "/" + name;
  }
  return std::string("/tmp/") + name;
}

std::string escapeJson(std::string_view text) {
  std::string out;
  out.reserve(text.size() + 8u);
  for (char c : text) {
    switch (c) {
    case '\\': out += "\\\\"; break;
    case '"': out += "\\\""; break;
    case '\n': out += "\\n"; break;
    case '\r': out += "\\r"; break;
    case '\t': out += "\\t"; break;
    default:
      if (static_cast<unsigned char>(c) >= 0x20u) out.push_back(c);
      break;
    }
  }
  return out;
}

bool lineContains(std::string_view line, std::string_view needle) {
  return line.find(needle) != std::string_view::npos;
}

std::string jsonStringField(std::string_view line, std::string_view name) {
  std::string const key = "\"" + std::string(name) + "\"";
  std::size_t pos = line.find(key);
  if (pos == std::string_view::npos) return {};
  pos = line.find(':', pos + key.size());
  if (pos == std::string_view::npos) return {};
  pos = line.find('"', pos + 1u);
  if (pos == std::string_view::npos) return {};
  std::string out;
  bool escaping = false;
  for (++pos; pos < line.size(); ++pos) {
    char const c = line[pos];
    if (escaping) {
      out.push_back(c);
      escaping = false;
    } else if (c == '\\') {
      escaping = true;
    } else if (c == '"') {
      break;
    } else {
      out.push_back(c);
    }
  }
  return out;
}

std::uint64_t jsonUintField(std::string_view line, std::string_view name) {
  std::string const key = "\"" + std::string(name) + "\"";
  std::size_t pos = line.find(key);
  if (pos == std::string_view::npos) return 0;
  pos = line.find(':', pos + key.size());
  if (pos == std::string_view::npos) return 0;
  while (++pos < line.size() && std::isspace(static_cast<unsigned char>(line[pos]))) {}
  std::uint64_t value = 0;
  while (pos < line.size() && std::isdigit(static_cast<unsigned char>(line[pos]))) {
    value = value * 10u + static_cast<unsigned>(line[pos] - '0');
    ++pos;
  }
  return value;
}

void sendLine(int fd, std::string const& line) {
  if (fd < 0) return;
  std::string payload = line;
  payload.push_back('\n');
  char const* data = payload.data();
  std::size_t remaining = payload.size();
  while (remaining > 0) {
    ssize_t const written = write(fd, data, remaining);
    if (written < 0) {
      if (errno == EINTR) continue;
      if (errno == EAGAIN || errno == EWOULDBLOCK) return;
      return;
    }
    data += written;
    remaining -= static_cast<std::size_t>(written);
  }
}

std::string appRegistryJson() {
  return R"([{"id":"files","name":"Files","command":"files"},{"id":"browser","name":"Browser","command":"browser"},{"id":"terminal","name":"Terminal","command":"terminal"},{"id":"settings","name":"Settings","command":"settings"},{"id":"calendar","name":"Calendar","command":"calendar"},{"id":"mail","name":"Mail","command":"mail"},{"id":"music","name":"Music","command":"music"}])";
}

std::string windowStateJson(WaylandServer::Impl const* server, WaylandServer::Impl::Surface const* surface) {
  auto* toplevel = toplevelForSurface(const_cast<WaylandServer::Impl*>(server),
                                      const_cast<WaylandServer::Impl::Surface*>(surface));
  std::string state = "normal";
  if (surface->minimized) state = "minimized";
  else if (surface->maximized) state = "maximized";
  std::string const appId = toplevel && !toplevel->appId.empty() ? toplevel->appId : "unknown";
  std::string const title = toplevel && !toplevel->title.empty() ? toplevel->title : appId;
  return "{\"id\":" + std::to_string(surface->id) +
         ",\"appId\":\"" + escapeJson(appId) +
         "\",\"title\":\"" + escapeJson(title) +
         "\",\"outputId\":\"" + escapeJson(server->output_.name) +
         "\",\"state\":\"" + state +
         "\",\"focused\":" + (server->keyboardFocus_ == surface ? "true" : "false") +
         ",\"attention\":false}";
}

std::string desktopSnapshotJson(WaylandServer::Impl const* server) {
  std::string json = "{\"type\":\"lambda.windowManager.snapshot\",\"outputs\":[{\"id\":\"" +
                     escapeJson(server->output_.name) + "\",\"name\":\"" +
                     escapeJson(server->output_.name) + "\",\"width\":" +
                     std::to_string(server->logicalOutputWidth()) + ",\"height\":" +
                     std::to_string(server->logicalOutputHeight()) + ",\"scale\":" +
                     std::to_string(server->preferredScale()) + "}],\"apps\":" +
                     appRegistryJson() + ",\"windows\":[";
  bool first = true;
  for (auto const& surface : server->surfaces_) {
    if (!surface || !surfaceIsXdgToplevel(surface.get())) continue;
    if (!first) json.push_back(',');
    first = false;
    json += windowStateJson(server, surface.get());
  }
  json += "],\"activeWindowId\":";
  json += server->keyboardFocus_ && surfaceIsXdgToplevel(server->keyboardFocus_)
              ? std::to_string(server->keyboardFocus_->id)
              : "null";
  json += ",\"system\":{\"network\":\"unknown\",\"wifi\":\"unknown\",\"bluetooth\":\"unknown\",";
  json += "\"volume\":\"unknown\",\"battery\":\"unknown\"}}";
  return json;
}

void closeShellClient(WaylandServer::Impl* server) {
  if (server->shellClientFd_ >= 0) close(server->shellClientFd_);
  server->shellClientFd_ = -1;
  server->shellReadBuffer_.clear();
  server->shellHelloReceived_ = false;
  server->shellSnapshotDirty_ = false;
}

void sendWelcome(WaylandServer::Impl* server) {
  sendLine(server->shellClientFd_,
           "{\"type\":\"lambda.windowManager.welcome\",\"protocolVersion\":1,"
           "\"sessionId\":\"lambda-session\",\"outputs\":[],"
           "\"theme\":{\"mode\":\"system\",\"accent\":\"#2a7fff\"}}");
  sendLine(server->shellClientFd_, desktopSnapshotJson(server));
}

void handleShellLine(WaylandServer::Impl* server, std::string_view line) {
  if (lineContains(line, "\"lambda.shell.hello\"")) {
    server->shellHelloReceived_ = true;
    sendWelcome(server);
    return;
  }
  if (lineContains(line, "\"lambda.windowManager.launchApp\"")) {
    server->launchShellApp(jsonStringField(line, "appId"));
    return;
  }
  if (lineContains(line, "\"lambda.windowManager.focusApp\"")) {
    if (!server->focusShellApp(jsonStringField(line, "appId"), 0)) {
      sendLine(server->shellClientFd_, "{\"type\":\"lambda.windowManager.error\",\"code\":\"not-found\",\"message\":\"app has no running windows\"}");
    }
    return;
  }
  if (lineContains(line, "\"lambda.windowManager.focusWindow\"")) {
    if (!server->focusShellWindow(jsonUintField(line, "windowId"), 0)) {
      sendLine(server->shellClientFd_, "{\"type\":\"lambda.windowManager.error\",\"code\":\"not-found\",\"message\":\"window not found\"}");
    }
    return;
  }
  if (lineContains(line, "\"lambda.windowManager.claimCommandLauncherModal\"")) {
    server->claimCommandLauncherModal(0);
    return;
  }
  if (lineContains(line, "\"lambda.windowManager.releaseCommandLauncherModal\"")) {
    server->releaseCommandLauncherModal(0);
    return;
  }
  if (lineContains(line, "\"lambda.shell.refreshState\"")) {
    sendLine(server->shellClientFd_, desktopSnapshotJson(server));
  }
}

} // namespace

int WaylandServer::Impl::shellIpcFd() const noexcept {
  return shellClientFd_ >= 0 ? shellClientFd_ : shellListenFd_;
}

void WaylandServer::Impl::initializeShellIpc() {
  shellSocketPath_ = runtimePath("lambda-window-manager-shell.sock");
  shellListenFd_ = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
  if (shellListenFd_ < 0) {
    std::fprintf(stderr, "lambda-window-manager: shell IPC socket failed: %s\n", std::strerror(errno));
    return;
  }
  sockaddr_un addr{};
  addr.sun_family = AF_UNIX;
  std::snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", shellSocketPath_.c_str());
  unlink(shellSocketPath_.c_str());
  if (bind(shellListenFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 ||
      listen(shellListenFd_, 1) != 0) {
    std::fprintf(stderr, "lambda-window-manager: shell IPC bind failed: %s\n", std::strerror(errno));
    close(shellListenFd_);
    shellListenFd_ = -1;
    return;
  }
  chmod(shellSocketPath_.c_str(), 0600);
  setenv("LAMBDA_SHELL_SOCKET", shellSocketPath_.c_str(), 1);
  std::fprintf(stderr, "lambda-window-manager: shell IPC %s\n", shellSocketPath_.c_str());
}

void WaylandServer::Impl::shutdownShellIpc() {
  closeShellClient(this);
  if (shellListenFd_ >= 0) close(shellListenFd_);
  shellListenFd_ = -1;
  if (!shellSocketPath_.empty()) unlink(shellSocketPath_.c_str());
}

void WaylandServer::Impl::dispatchShellIpc() {
  if (shellListenFd_ >= 0 && shellClientFd_ < 0) {
    int fd = accept4(shellListenFd_, nullptr, nullptr, SOCK_CLOEXEC | SOCK_NONBLOCK);
    if (fd >= 0) {
      shellClientFd_ = fd;
      shellReadBuffer_.clear();
      shellHelloReceived_ = false;
      std::fprintf(stderr, "lambda-window-manager: lambda-shell connected\n");
    }
  }
  if (shellClientFd_ < 0) return;

  char buffer[4096];
  for (;;) {
    ssize_t const readBytes = read(shellClientFd_, buffer, sizeof(buffer));
    if (readBytes < 0) {
      if (errno == EINTR) continue;
      if (errno == EAGAIN || errno == EWOULDBLOCK) break;
      closeShellClient(this);
      return;
    }
    if (readBytes == 0) {
      closeShellClient(this);
      return;
    }
    shellReadBuffer_.append(buffer, static_cast<std::size_t>(readBytes));
    for (;;) {
      std::size_t const newline = shellReadBuffer_.find('\n');
      if (newline == std::string::npos) break;
      std::string line = shellReadBuffer_.substr(0, newline);
      shellReadBuffer_.erase(0, newline + 1u);
      handleShellLine(this, line);
    }
  }
  if (shellHelloReceived_ && shellSnapshotDirty_) {
    shellSnapshotDirty_ = false;
    sendLine(shellClientFd_, desktopSnapshotJson(this));
  }
}

void WaylandServer::Impl::requestShellOpenCommandLauncher() {
  if (shellClientFd_ < 0 || !shellHelloReceived_) {
    std::fprintf(stderr, "lambda-window-manager: lambda-shell is not connected; cannot open command launcher\n");
    return;
  }
  sendLine(shellClientFd_, "{\"type\":\"lambda.shell.openCommandLauncher\"}");
}

void WaylandServer::Impl::notifyShellStateChanged() {
  shellSnapshotDirty_ = true;
}

} // namespace flux::compositor
