#pragma once

/// \file Flux/UI/Clipboard.hpp
///
/// Part of the Flux public API.


#include <optional>
#include <string>

namespace flux {

/// Platform clipboard. Access via Application::clipboard().
///
/// All methods must be called from the main thread.
class Clipboard {
public:
  virtual ~Clipboard() = default;

  /// Returns the current plain-text content of the system clipboard,
  /// or nullopt if the clipboard is empty or contains no plain-text data.
  virtual std::optional<std::string> readText() const = 0;

  /// Replaces the system clipboard contents with text (UTF-8).
  /// Passing an empty string clears the clipboard.
  virtual void writeText(std::string text) = 0;

  /// True if the clipboard currently holds plain-text data.
  /// Cheaper than readText() when only presence needs to be checked.
  virtual bool hasText() const = 0;
};

} // namespace flux
