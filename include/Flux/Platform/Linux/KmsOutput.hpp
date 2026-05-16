#pragma once

/// \file Flux/Platform/Linux/KmsOutput.hpp
///
/// Linux-only KMS output access for embedders that need to own a display without
/// creating a Flux Window.

#if FLUX_VULKAN

#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

namespace flux::platform {

class KmsOutput;

class KmsDevice {
public:
  class Impl;

  static std::unique_ptr<KmsDevice> open(char const* devicePath = nullptr);

  ~KmsDevice();

  KmsDevice(KmsDevice const&) = delete;
  KmsDevice& operator=(KmsDevice const&) = delete;
  KmsDevice(KmsDevice&&) noexcept;
  KmsDevice& operator=(KmsDevice&&) noexcept;

  [[nodiscard]] std::vector<KmsOutput> outputs() const;
  [[nodiscard]] int fd() const noexcept;
  [[nodiscard]] std::span<char const* const> requiredVulkanInstanceExtensions() const;
  [[nodiscard]] std::filesystem::path cacheDir() const;
  [[nodiscard]] bool isVtForeground() const noexcept;
  [[nodiscard]] bool shouldTerminate() const noexcept;

  /// Services signal, VT-switch, input, wake, and hotplug events owned by the KMS device.
  bool pollEvents(int timeoutMs = 0);

private:
  explicit KmsDevice(std::shared_ptr<Impl> impl);

  std::shared_ptr<Impl> impl_;
};

class KmsOutput {
public:
  KmsOutput();
  ~KmsOutput();

  KmsOutput(KmsOutput const&);
  KmsOutput& operator=(KmsOutput const&);
  KmsOutput(KmsOutput&&) noexcept;
  KmsOutput& operator=(KmsOutput&&) noexcept;

  [[nodiscard]] std::string const& name() const noexcept;
  [[nodiscard]] std::uint32_t width() const noexcept;
  [[nodiscard]] std::uint32_t height() const noexcept;
  [[nodiscard]] std::uint32_t refreshRateMilliHz() const noexcept;

  [[nodiscard]] VkSurfaceKHR createVulkanSurface(VkInstance instance) const;

  /// Lightweight vblank pacing approximation used by phase-1 compositor code.
  /// The KMS Window path still uses its existing frame scheduling.
  void waitForVblank() const;

private:
  class Impl;

  explicit KmsOutput(std::shared_ptr<Impl> impl);

  std::shared_ptr<Impl> impl_;

  friend class KmsDevice::Impl;
};

} // namespace flux::platform

#endif // FLUX_VULKAN
