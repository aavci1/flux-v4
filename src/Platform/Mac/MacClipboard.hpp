#pragma once

#include <Flux/Core/Clipboard.hpp>

namespace flux {

class MacClipboard final : public Clipboard {
public:
  std::optional<std::string> readText() const override;
  void writeText(std::string text) override;
  bool hasText() const override;
};

} // namespace flux
