#include "Testing/TestServer.hpp"

#include <Flux/Core/Application.hpp>
#include <Flux/Core/Window.hpp>

#include "Core/PlatformWindow.hpp"
#include <Flux/Core/EventQueue.hpp>
#include <Flux/Core/KeyCodes.hpp>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace flux {

namespace {

constexpr std::uint8_t kPressedLeft = 1u;

} // namespace

TestServer::TestServer(Window& window, int tcpPort, std::string unixSocketPath)
    : window_(window)
    , tcpPort_(tcpPort)
    , unixSocketPath_(std::move(unixSocketPath)) {}

TestServer::~TestServer() { stop(); }

void TestServer::start() {
  if (running_.exchange(true)) {
    return;
  }
  thread_ = std::thread([this]() { run(); });
  if (!unixSocketPath_.empty()) {
    std::cout << "\n  FLUX TEST IPC (unix): " << unixSocketPath_ << "\n"
              << "  Binary ops: GetUi GetScreenshot Click Type Key Scroll Hover Drag\n"
              << std::endl;
  } else {
    std::cout << "\n  FLUX TEST IPC (tcp): localhost:" << tcpPort_ << "\n"
              << "  Binary ops: GetUi GetScreenshot Click Type Key Scroll Hover Drag\n"
              << std::endl;
  }
}

void TestServer::stop() {
  running_ = false;
  {
    std::lock_guard<std::mutex> lock(screenshotWaitMutex_);
    screenshotCv_.notify_all();
  }
  if (serverFd_ >= 0) {
    shutdown(serverFd_, SHUT_RDWR);
    ::close(serverFd_);
    serverFd_ = -1;
  }
  if (thread_.joinable()) {
    thread_.join();
  }
  if (!unixSocketPath_.empty()) {
    ::unlink(unixSocketPath_.c_str());
  }
}

void TestServer::setUiTreeJson(std::string json) {
  std::lock_guard<std::mutex> lock(snapshotMutex_);
  uiTreeJSON_ = std::move(json);
}

void TestServer::setLastScreenshotPng(std::vector<std::uint8_t> png) {
  {
    std::lock_guard<std::mutex> lock(pngMutex_);
    lastPng_ = std::move(png);
  }
  screenshotDoneSeq_.store(screenshotReqSeq_.load(std::memory_order_acquire), std::memory_order_release);
  {
    std::lock_guard<std::mutex> lock(screenshotWaitMutex_);
    screenshotCv_.notify_all();
  }
}

bool TestServer::readFull(int fd, void* dest, std::size_t n) {
  auto* p = static_cast<std::uint8_t*>(dest);
  std::size_t got = 0;
  while (got < n) {
    ssize_t r = recv(fd, p + got, n - got, 0);
    if (r <= 0) {
      return false;
    }
    got += static_cast<std::size_t>(r);
  }
  return true;
}

bool TestServer::sendAll(int fd, void const* data, std::size_t n) {
  auto const* p = static_cast<std::uint8_t const*>(data);
  std::size_t sent = 0;
  while (sent < n) {
    ssize_t w = send(fd, p + sent, n - sent, 0);
    if (w <= 0) {
      return false;
    }
    sent += static_cast<std::size_t>(w);
  }
  return true;
}

void TestServer::sendBinaryResponse(int fd, std::uint8_t status, std::uint8_t payloadType, void const* body,
                                    std::size_t bodyLen) {
  std::uint8_t hdr[8];
  hdr[0] = status;
  hdr[1] = payloadType;
  hdr[2] = 0;
  hdr[3] = 0;
  std::uint32_t len32 = static_cast<std::uint32_t>(bodyLen);
  std::memcpy(hdr + 4, &len32, sizeof(len32));
  if (!sendAll(fd, hdr, sizeof(hdr))) {
    return;
  }
  if (bodyLen > 0) {
    sendAll(fd, body, bodyLen);
  }
}

void TestServer::postInput(InputEvent ev) {
  Application::instance().eventQueue().post(std::move(ev));
  if (auto* pw = window_.platformWindow()) {
    pw->wakeEventLoop();
  }
}

void TestServer::waitForNextPresent() {
  std::uint64_t const before = Application::instance().testPresentGeneration();
  Application::instance().requestRedraw();
  if (auto* pw = window_.platformWindow()) {
    pw->wakeEventLoop();
  }
  Application::instance().waitForTestPresentAfter(before, 3000);
}

void TestServer::sendGetUi(int fd) {
  std::string json;
  {
    std::lock_guard<std::mutex> lock(snapshotMutex_);
    json = uiTreeJSON_;
  }
  if (json.empty()) {
    json = "{}";
  }
  sendBinaryResponse(fd, 0, 0, json.data(), json.size());
}

void TestServer::sendScreenshotBinary(int fd) {
  std::uint64_t const myReq = screenshotReqSeq_.fetch_add(1, std::memory_order_acq_rel) + 1;
  requestCaptureNextFrame();
  Application::instance().requestRedraw();
  if (auto* pw = window_.platformWindow()) {
    pw->wakeEventLoop();
  }

  std::unique_lock<std::mutex> lock(screenshotWaitMutex_);
  bool ok = screenshotCv_.wait_for(lock, std::chrono::seconds(5), [&]() {
    return screenshotDoneSeq_.load(std::memory_order_acquire) >= myReq;
  });
  if (!ok) {
    std::string const msg = R"({"error":"screenshot timeout"})";
    sendBinaryResponse(fd, 1, 0, msg.data(), msg.size());
    return;
  }
  std::vector<std::uint8_t> png;
  {
    std::lock_guard<std::mutex> plock(pngMutex_);
    png = lastPng_;
  }
  if (png.empty()) {
    std::string const msg = R"({"error":"no screenshot"})";
    sendBinaryResponse(fd, 1, 0, msg.data(), msg.size());
    return;
  }
  sendBinaryResponse(fd, 0, 1, png.data(), png.size());
}

void TestServer::handleClickBinary(int fd, std::string const& body) {
  float x = 0, y = 0;
  parseFloat(body, "x", x);
  parseFloat(body, "y", y);
  unsigned int const h = window_.handle();
  {
    InputEvent down{};
    down.kind = InputEvent::Kind::PointerDown;
    down.handle = h;
    down.position = {x, y};
    down.button = MouseButton::Left;
    down.pressedButtons = kPressedLeft;
    postInput(down);
  }
  {
    InputEvent up{};
    up.kind = InputEvent::Kind::PointerUp;
    up.handle = h;
    up.position = {x, y};
    up.button = MouseButton::Left;
    up.pressedButtons = 0;
    postInput(up);
  }
  waitForNextPresent();
  std::string resp = R"({"ok":true,"action":"click","x":)" + std::to_string(x) + R"(,"y":)" + std::to_string(y) + "}";
  sendBinaryResponse(fd, 0, 0, resp.data(), resp.size());
}

void TestServer::handleTypeBinary(int fd, std::string const& body) {
  std::string text = parseString(body, "text");
  unsigned int const h = window_.handle();
  for (char c : text) {
    InputEvent ev{};
    ev.kind = InputEvent::Kind::TextInput;
    ev.handle = h;
    ev.text = std::string(1, c);
    postInput(ev);
  }
  waitForNextPresent();
  std::string const resp = R"({"ok":true,"action":"type"})";
  sendBinaryResponse(fd, 0, 0, resp.data(), resp.size());
}

void TestServer::handleKeyBinary(int fd, std::string const& body) {
  std::string keyStr = parseString(body, "key");
  KeyCode const code = keyNameToCode(keyStr);
  Modifiers mods = Modifiers::None;
  if (body.find("\"modifiers\"") != std::string::npos) {
    mods = parseModifiersList(body);
  }
  unsigned int const h = window_.handle();
  {
    InputEvent down{};
    down.kind = InputEvent::Kind::KeyDown;
    down.handle = h;
    down.key = code;
    down.modifiers = mods;
    postInput(down);
  }
  {
    InputEvent up{};
    up.kind = InputEvent::Kind::KeyUp;
    up.handle = h;
    up.key = code;
    up.modifiers = mods;
    postInput(up);
  }
  waitForNextPresent();
  std::string resp = R"({"ok":true,"action":"key","key":")" + escapeJson(keyStr) + "\"}";
  sendBinaryResponse(fd, 0, 0, resp.data(), resp.size());
}

void TestServer::handleScrollBinary(int fd, std::string const& body) {
  float x = 0, y = 0, dx = 0, dy = 0;
  parseFloat(body, "x", x);
  parseFloat(body, "y", y);
  parseFloat(body, "deltaX", dx);
  parseFloat(body, "deltaY", dy);
  InputEvent ev{};
  ev.kind = InputEvent::Kind::Scroll;
  ev.handle = window_.handle();
  ev.position = {x, y};
  ev.scrollDelta = {dx, dy};
  ev.preciseScrollDelta = true;
  postInput(ev);
  waitForNextPresent();
  std::string const resp = R"({"ok":true,"action":"scroll"})";
  sendBinaryResponse(fd, 0, 0, resp.data(), resp.size());
}

void TestServer::handleHoverBinary(int fd, std::string const& body) {
  float x = 0, y = 0;
  parseFloat(body, "x", x);
  parseFloat(body, "y", y);
  InputEvent ev{};
  ev.kind = InputEvent::Kind::PointerMove;
  ev.handle = window_.handle();
  ev.position = {x, y};
  ev.button = MouseButton::None;
  postInput(ev);
  waitForNextPresent();
  std::string const resp = R"({"ok":true,"action":"hover"})";
  sendBinaryResponse(fd, 0, 0, resp.data(), resp.size());
}

void TestServer::handleDragBinary(int fd, std::string const& body) {
  float startX = 0, startY = 0, endX = 0, endY = 0;
  float steps = 10;
  parseFloat(body, "startX", startX);
  parseFloat(body, "startY", startY);
  parseFloat(body, "endX", endX);
  parseFloat(body, "endY", endY);
  parseFloat(body, "steps", steps);
  int const numSteps = std::max(1, static_cast<int>(steps));
  unsigned int const h = window_.handle();

  {
    InputEvent down{};
    down.kind = InputEvent::Kind::PointerDown;
    down.handle = h;
    down.position = {startX, startY};
    down.button = MouseButton::Left;
    down.pressedButtons = kPressedLeft;
    postInput(down);
  }
  waitForNextPresent();

  for (int i = 1; i <= numSteps; ++i) {
    float const t = static_cast<float>(i) / static_cast<float>(numSteps);
    float const mx = startX + (endX - startX) * t;
    float const my = startY + (endY - startY) * t;
    InputEvent mv{};
    mv.kind = InputEvent::Kind::PointerMove;
    mv.handle = h;
    mv.position = {mx, my};
    mv.button = MouseButton::None;
    mv.pressedButtons = kPressedLeft;
    postInput(mv);
    waitForNextPresent();
  }

  {
    InputEvent up{};
    up.kind = InputEvent::Kind::PointerUp;
    up.handle = h;
    up.position = {endX, endY};
    up.button = MouseButton::Left;
    up.pressedButtons = 0;
    postInput(up);
  }
  waitForNextPresent();

  std::string resp = R"({"ok":true,"action":"drag","startX":)" + std::to_string(startX) + R"(,"startY":)" +
                     std::to_string(startY) + R"(,"endX":)" + std::to_string(endX) + R"(,"endY":)" +
                     std::to_string(endY) + "}";
  sendBinaryResponse(fd, 0, 0, resp.data(), resp.size());
}

void TestServer::parseFloat(std::string const& json, std::string const& key, float& out) {
  std::string const needle = "\"" + key + "\"";
  auto pos = json.find(needle);
  if (pos == std::string::npos) {
    return;
  }
  pos = json.find(':', pos + needle.size());
  if (pos == std::string::npos) {
    return;
  }
  ++pos;
  while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) {
    ++pos;
  }
  try {
    out = std::stof(json.substr(pos));
  } catch (...) {
  }
}

std::string TestServer::parseString(std::string const& json, std::string const& key) {
  std::string const needle = "\"" + key + "\"";
  auto pos = json.find(needle);
  if (pos == std::string::npos) {
    return "";
  }
  pos = json.find(':', pos + needle.size());
  if (pos == std::string::npos) {
    return "";
  }
  pos = json.find('"', pos + 1);
  if (pos == std::string::npos) {
    return "";
  }
  ++pos;
  auto end = json.find('"', pos);
  if (end == std::string::npos) {
    return "";
  }
  return json.substr(pos, end - pos);
}

Modifiers TestServer::parseModifiersList(std::string const& json) {
  std::uint32_t bits = 0;
  auto pos = json.find("\"modifiers\"");
  if (pos == std::string::npos) {
    return Modifiers::None;
  }
  pos = json.find('[', pos);
  if (pos == std::string::npos) {
    return Modifiers::None;
  }
  auto const end = json.find(']', pos);
  if (end == std::string::npos) {
    return Modifiers::None;
  }
  std::string const inner = json.substr(pos + 1, end - pos - 1);
  auto has = [&](char const* token) { return inner.find(token) != std::string::npos; };
  if (has("ctrl") || has("Ctrl") || has("control") || has("Control")) {
    bits |= static_cast<std::uint32_t>(Modifiers::Ctrl);
  }
  if (has("shift") || has("Shift")) {
    bits |= static_cast<std::uint32_t>(Modifiers::Shift);
  }
  if (has("alt") || has("Alt") || has("option") || has("Option")) {
    bits |= static_cast<std::uint32_t>(Modifiers::Alt);
  }
  if (has("super") || has("Super") || has("meta") || has("Meta") || has("cmd") || has("Command")) {
    bits |= static_cast<std::uint32_t>(Modifiers::Meta);
  }
  return static_cast<Modifiers>(bits);
}

std::string TestServer::escapeJson(std::string const& s) {
  std::string out;
  out.reserve(s.size());
  for (char c : s) {
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        out += c;
        break;
    }
  }
  return out;
}

KeyCode TestServer::keyNameToCode(std::string const& name) {
  using namespace flux::keys;
  if (name == "Enter" || name == "Return") {
    return Return;
  }
  if (name == "Tab") {
    return Tab;
  }
  if (name == "Backspace" || name == "Delete") {
    return Delete;
  }
  if (name == "Escape") {
    return Escape;
  }
  if (name == "Space") {
    return Space;
  }
  if (name == "Left") {
    return LeftArrow;
  }
  if (name == "Right") {
    return RightArrow;
  }
  if (name == "Up") {
    return UpArrow;
  }
  if (name == "Down") {
    return DownArrow;
  }
  if (name == "Home") {
    return Home;
  }
  if (name == "End") {
    return End;
  }
  if (name == "PageUp") {
    return PageUp;
  }
  if (name == "PageDown") {
    return PageDown;
  }
  if (name.size() == 1 && name[0] >= 'A' && name[0] <= 'Z') {
    return static_cast<KeyCode>(A + (name[0] - 'A'));
  }
  if (name.size() == 1 && name[0] >= 'a' && name[0] <= 'z') {
    return static_cast<KeyCode>(A + (name[0] - 'a'));
  }
  if (name.size() == 1 && name[0] >= '0' && name[0] <= '9') {
    // macOS virtual key codes (ANSI)
    static constexpr KeyCode kDigit[10] = {0x1D, 0x12, 0x13, 0x14, 0x15, 0x17, 0x16, 0x1A, 0x1C, 0x19};
    return kDigit[static_cast<unsigned>(name[0] - '0')];
  }
  return 0;
}

void TestServer::run() {
  if (!unixSocketPath_.empty()) {
    serverFd_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (serverFd_ < 0) {
      std::cerr << "TestServer: unix socket failed\n";
      return;
    }
    ::unlink(unixSocketPath_.c_str());
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    if (unixSocketPath_.size() >= sizeof(addr.sun_path)) {
      std::cerr << "TestServer: unix socket path too long\n";
      ::close(serverFd_);
      serverFd_ = -1;
      return;
    }
    std::strncpy(addr.sun_path, unixSocketPath_.c_str(), sizeof(addr.sun_path) - 1);
    if (bind(serverFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
      std::cerr << "TestServer: unix bind failed\n";
      ::close(serverFd_);
      serverFd_ = -1;
      return;
    }
  } else {
    serverFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd_ < 0) {
      std::cerr << "TestServer: socket failed\n";
      return;
    }
    int opt = 1;
    setsockopt(serverFd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(static_cast<std::uint16_t>(tcpPort_));
    if (bind(serverFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
      std::cerr << "TestServer: bind failed on port " << tcpPort_ << "\n";
      ::close(serverFd_);
      serverFd_ = -1;
      return;
    }
  }

  listen(serverFd_, 8);

  while (running_) {
    int clientFd = accept(serverFd_, nullptr, nullptr);
    if (clientFd < 0) {
      continue;
    }
    handleBinaryClient(clientFd);
    shutdown(clientFd, SHUT_RDWR);
    ::close(clientFd);
  }
}

void TestServer::handleBinaryClient(int fd) {
  std::uint32_t magic = 0;
  std::uint16_t version = 0;
  std::uint16_t opcode = 0;
  std::uint32_t bodyLen = 0;
  if (!readFull(fd, &magic, sizeof(magic))) {
    return;
  }
  if (!readFull(fd, &version, sizeof(version))) {
    return;
  }
  if (!readFull(fd, &opcode, sizeof(opcode))) {
    return;
  }
  if (!readFull(fd, &bodyLen, sizeof(bodyLen))) {
    return;
  }
  if (magic != kFluxTestMagic || version != 1) {
    std::string const err = R"({"error":"bad request"})";
    sendBinaryResponse(fd, 1, 0, err.data(), err.size());
    return;
  }
  constexpr std::uint32_t kMaxBody = 4 * 1024 * 1024;
  if (bodyLen > kMaxBody) {
    std::string const err = R"({"error":"body too large"})";
    sendBinaryResponse(fd, 1, 0, err.data(), err.size());
    return;
  }
  std::string body;
  if (bodyLen > 0) {
    body.resize(bodyLen);
    if (!readFull(fd, body.data(), bodyLen)) {
      return;
    }
  }

  switch (opcode) {
  case static_cast<std::uint16_t>(Op::GetUi):
    sendGetUi(fd);
    break;
  case static_cast<std::uint16_t>(Op::GetScreenshot):
    sendScreenshotBinary(fd);
    break;
  case static_cast<std::uint16_t>(Op::Click):
    handleClickBinary(fd, body);
    break;
  case static_cast<std::uint16_t>(Op::Type):
    handleTypeBinary(fd, body);
    break;
  case static_cast<std::uint16_t>(Op::Key):
    handleKeyBinary(fd, body);
    break;
  case static_cast<std::uint16_t>(Op::Scroll):
    handleScrollBinary(fd, body);
    break;
  case static_cast<std::uint16_t>(Op::Hover):
    handleHoverBinary(fd, body);
    break;
  case static_cast<std::uint16_t>(Op::Drag):
    handleDragBinary(fd, body);
    break;
  default: {
    std::string const msg = R"({"error":"unknown opcode"})";
    sendBinaryResponse(fd, 1, 0, msg.data(), msg.size());
    break;
  }
  }
}

} // namespace flux
