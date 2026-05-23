#include "Compositor/PresentationLoop.hpp"

#include <charconv>
#include <cctype>
#include <cstdio>
#include <string>
#include <string_view>

namespace flux::compositor::presentation {
namespace {

std::string lowerAscii(std::string_view value) {
  std::string result(value);
  for (char& c : result) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return result;
}

std::optional<std::size_t> parseOutputIndex(std::string_view selector, std::size_t count) {
  std::size_t index = 0;
  auto const* begin = selector.data();
  auto const* end = selector.data() + selector.size();
  auto [ptr, error] = std::from_chars(begin, end, index);
  if (error != std::errc{} || ptr != end || index >= count) return std::nullopt;
  return index;
}

} // namespace

PresentationTiming presentationTimingFromVblank(platform::KmsOutput::VblankTiming const& vblank,
                                                std::uint32_t refreshMilliHz,
                                                std::uint64_t fallbackSequence) {
  return PresentationTiming{
      .monotonicNsec = vblank.monotonicNsec != 0 ? vblank.monotonicNsec : monotonicNanoseconds(),
      .sequence = vblank.hardware ? vblank.sequence : fallbackSequence,
      .refreshNsec = refreshNsec(refreshMilliHz),
      .flags = vblank.hardware ? static_cast<std::uint32_t>(WP_PRESENTATION_FEEDBACK_KIND_VSYNC |
                                                            WP_PRESENTATION_FEEDBACK_KIND_HW_CLOCK)
                               : 0u,
  };
}

void printOutputs(std::vector<flux::platform::KmsOutput> const& outputs) {
  std::fprintf(stderr, "lambda-window-manager: connected KMS outputs:\n");
  for (std::size_t i = 0; i < outputs.size(); ++i) {
    auto const& output = outputs[i];
    std::fprintf(stderr,
                 "lambda-window-manager:   [%zu] %s %ux%u @ %.3f Hz\n",
                 i,
                 output.name().c_str(),
                 output.width(),
                 output.height(),
                 static_cast<double>(output.refreshRateMilliHz()) / 1000.0);
  }
}

std::optional<std::size_t> selectOutputIndex(std::vector<flux::platform::KmsOutput> const& outputs,
                                             std::optional<std::string> const& selector) {
  if (outputs.empty()) return std::nullopt;
  if (!selector || selector->empty()) return 0u;

  for (std::size_t i = 0; i < outputs.size(); ++i) {
    if (outputs[i].name() == *selector) return i;
  }

  std::string const normalized = lowerAscii(*selector);
  for (std::size_t i = 0; i < outputs.size(); ++i) {
    if (lowerAscii(outputs[i].name()) == normalized) return i;
  }
  if (normalized == "primary" || normalized == "first") return 0u;
  if ((normalized == "secondary" || normalized == "second") && outputs.size() > 1u) return 1u;
  return parseOutputIndex(*selector, outputs.size());
}

} // namespace flux::compositor::presentation
