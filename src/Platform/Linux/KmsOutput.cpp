#include <Flux/Platform/Linux/KmsOutput.hpp>

#include "Platform/Linux/KmsPlatform.hpp"

#include <algorithm>
#include <chrono>
#include <atomic>
#include <stdexcept>
#include <thread>
#include <utility>

namespace flux::platform {
namespace {

std::uint32_t refreshRateMilliHz(drmModeModeInfo const& mode) {
  if (mode.vrefresh > 0) return static_cast<std::uint32_t>(mode.vrefresh) * 1000u;
  if (mode.clock > 0 && mode.htotal > 0 && mode.vtotal > 0) {
    return static_cast<std::uint32_t>(
        (static_cast<std::uint64_t>(mode.clock) * 1'000'000ull) /
        (static_cast<std::uint64_t>(mode.htotal) * static_cast<std::uint64_t>(mode.vtotal)));
  }
  return 60'000u;
}

std::chrono::nanoseconds frameInterval(std::uint32_t refreshMilliHz) {
  if (refreshMilliHz == 0) refreshMilliHz = 60'000u;
  return std::chrono::nanoseconds(1'000'000'000'000ll / refreshMilliHz);
}

} // namespace

class KmsDevice::Impl {
public:
  explicit Impl(char const* devicePath);

  std::vector<KmsOutput> outputs(std::shared_ptr<Impl> self) const;

  std::unique_ptr<flux::KmsApplication> app_;
};

class KmsOutput::Impl {
public:
  Impl(std::shared_ptr<KmsDevice::Impl> device, KmsConnector connector)
      : device_(std::move(device)), connector_(std::move(connector)) {}

  std::shared_ptr<KmsDevice::Impl> device_;
  KmsConnector connector_{};
};

KmsOutput::KmsOutput() = default;
KmsOutput::~KmsOutput() = default;
KmsOutput::KmsOutput(KmsOutput const&) = default;
KmsOutput& KmsOutput::operator=(KmsOutput const&) = default;
KmsOutput::KmsOutput(KmsOutput&&) noexcept = default;
KmsOutput& KmsOutput::operator=(KmsOutput&&) noexcept = default;

KmsOutput::KmsOutput(std::shared_ptr<Impl> impl) : impl_(std::move(impl)) {}

std::string const& KmsOutput::name() const noexcept {
  static std::string const empty;
  return impl_ ? impl_->connector_.name : empty;
}

std::uint32_t KmsOutput::width() const noexcept {
  return impl_ ? impl_->connector_.mode.hdisplay : 0u;
}

std::uint32_t KmsOutput::height() const noexcept {
  return impl_ ? impl_->connector_.mode.vdisplay : 0u;
}

std::uint32_t KmsOutput::refreshRateMilliHz() const noexcept {
  return impl_ ? flux::platform::refreshRateMilliHz(impl_->connector_.mode) : 0u;
}

VkSurfaceKHR KmsOutput::createVulkanSurface(VkInstance instance) const {
  if (!impl_ || !impl_->device_ || !impl_->device_->app_) throw std::runtime_error("Invalid KMS output");
  return impl_->device_->app_->createVulkanSurface(instance, &impl_->connector_);
}

void KmsOutput::waitForVblank() const {
  std::this_thread::sleep_for(frameInterval(refreshRateMilliHz()));
}

KmsDevice::Impl::Impl(char const* devicePath) {
  if (devicePath && *devicePath) {
    throw std::runtime_error("KmsDevice::open(devicePath) is not implemented yet; pass nullptr");
  }
  app_ = std::make_unique<flux::KmsApplication>();
  app_->setApplicationName("flux-compositor");
  app_->initialize();
}

std::vector<KmsOutput> KmsDevice::Impl::outputs(std::shared_ptr<Impl> self) const {
  std::vector<KmsOutput> result;
  result.reserve(app_->connectors_.size());
  for (KmsConnector const& connector : app_->connectors_) {
    result.push_back(KmsOutput(std::shared_ptr<KmsOutput::Impl>(new KmsOutput::Impl(self, connector))));
  }
  return result;
}

std::unique_ptr<KmsDevice> KmsDevice::open(char const* devicePath) {
  return std::unique_ptr<KmsDevice>(new KmsDevice(std::make_shared<Impl>(devicePath)));
}

KmsDevice::KmsDevice(std::shared_ptr<Impl> impl) : impl_(std::move(impl)) {}
KmsDevice::~KmsDevice() = default;
KmsDevice::KmsDevice(KmsDevice&&) noexcept = default;
KmsDevice& KmsDevice::operator=(KmsDevice&&) noexcept = default;

std::vector<KmsOutput> KmsDevice::outputs() const {
  return impl_ ? impl_->outputs(impl_) : std::vector<KmsOutput>{};
}

int KmsDevice::fd() const noexcept {
  return impl_ && impl_->app_ ? impl_->app_->drmFd() : -1;
}

std::span<char const* const> KmsDevice::requiredVulkanInstanceExtensions() const {
  static std::vector<char const*> empty;
  return impl_ && impl_->app_ ? impl_->app_->requiredVulkanInstanceExtensions()
                              : std::span<char const* const>(empty.data(), empty.size());
}

std::filesystem::path KmsDevice::cacheDir() const {
  return impl_ && impl_->app_ ? std::filesystem::path(impl_->app_->cacheDir()) : std::filesystem::path{};
}

bool KmsDevice::isVtForeground() const noexcept {
  return impl_ && impl_->app_ ? impl_->app_->isVtForeground() : false;
}

bool KmsDevice::shouldTerminate() const noexcept {
  return impl_ && impl_->app_ ? impl_->app_->terminateRequested_.load(std::memory_order_relaxed) : true;
}

void KmsDevice::setInputHandler(std::function<void(KmsInputEvent const&)> handler) {
  if (impl_ && impl_->app_) impl_->app_->rawInputHandler_ = std::move(handler);
}

bool KmsDevice::pollEvents(int timeoutMs) {
  return impl_ && impl_->app_ ? impl_->app_->pollInputAndWake(timeoutMs) : false;
}

} // namespace flux::platform
