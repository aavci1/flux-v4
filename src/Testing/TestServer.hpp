#pragma once

#include <Flux/Core/Events.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace flux {

class Window;

/// Binary IPC server for `--test-mode` (same wire format as Flux v1).
class TestServer {
public:
  static constexpr std::uint32_t kFluxTestMagic = 0x58554C46;

  enum class Op : std::uint16_t {
    GetUi = 1,
    GetScreenshot = 2,
    Click = 3,
    Type = 4,
    Key = 5,
    Scroll = 6,
    Hover = 7,
    Drag = 8,
  };

  TestServer(Window& window, int tcpPort = 8435, std::string unixSocketPath = {});
  ~TestServer();

  void start();
  void stop();

  void setUiTreeJson(std::string json);

  void setLastScreenshotPng(std::vector<std::uint8_t> png);

  bool needsScreenshotCapture() const {
    return screenshotReqSeq_.load(std::memory_order_acquire) > screenshotDoneSeq_.load(std::memory_order_acquire);
  }

  void requestCaptureNextFrame() { captureNextFrame_.store(true, std::memory_order_release); }

  bool takeCaptureNextFrame() { return captureNextFrame_.exchange(false, std::memory_order_acq_rel); }

private:
  void run();
  void handleBinaryClient(int fd);

  void sendGetUi(int fd);
  void sendScreenshotBinary(int fd);

  void handleClickBinary(int fd, std::string const& body);
  void handleTypeBinary(int fd, std::string const& body);
  void handleKeyBinary(int fd, std::string const& body);
  void handleScrollBinary(int fd, std::string const& body);
  void handleHoverBinary(int fd, std::string const& body);
  void handleDragBinary(int fd, std::string const& body);

  void postInput(InputEvent ev);
  void waitForNextPresent();

  static bool readFull(int fd, void* dest, std::size_t n);
  static bool sendAll(int fd, void const* data, std::size_t n);
  void sendBinaryResponse(int fd, std::uint8_t status, std::uint8_t payloadType, void const* body, std::size_t bodyLen);

  static void parseFloat(std::string const& json, std::string const& key, float& out);
  static std::string parseString(std::string const& json, std::string const& key);
  static Modifiers parseModifiersList(std::string const& json);
  static std::string escapeJson(std::string const& s);
  static KeyCode keyNameToCode(std::string const& name);

  Window& window_;

  int tcpPort_{8435};
  std::string unixSocketPath_{};

  std::atomic<bool> running_{false};
  std::thread thread_;
  int serverFd_{-1};

  std::mutex snapshotMutex_;
  std::string uiTreeJSON_;
  std::atomic<std::uint64_t> screenshotReqSeq_{0};
  std::atomic<std::uint64_t> screenshotDoneSeq_{0};
  std::mutex screenshotWaitMutex_;
  std::condition_variable screenshotCv_;

  std::atomic<bool> captureNextFrame_{false};

  std::mutex pngMutex_;
  std::vector<std::uint8_t> lastPng_;
};

} // namespace flux
