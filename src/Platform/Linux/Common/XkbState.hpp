#pragma once

#include <Flux/UI/Input.hpp>

#include <cstdint>
#include <string>

struct xkb_context;
struct xkb_keymap;
struct xkb_state;

namespace flux::linux_platform {

class XkbState {
public:
  XkbState();
  ~XkbState();

  XkbState(XkbState const&) = delete;
  XkbState& operator=(XkbState const&) = delete;

  bool loadKeymapFromFd(int fd, std::uint32_t size);
  bool createDefaultKeymap();
  void updateModifiers(std::uint32_t depressed, std::uint32_t latched,
                       std::uint32_t locked, std::uint32_t group);
  void updateKey(std::uint32_t key, bool pressed);

  KeyCode keyCodeForEvdevKey(std::uint32_t key) const;
  std::string utf8ForEvdevKey(std::uint32_t key) const;
  Modifiers modifiers() const noexcept { return modifiers_; }

private:
  std::uint32_t keysymForEvdevKey(std::uint32_t key) const;
  bool installKeymap(xkb_keymap* keymap);

  xkb_context* context_ = nullptr;
  xkb_keymap* keymap_ = nullptr;
  xkb_state* state_ = nullptr;
  Modifiers modifiers_ = Modifiers::None;
};

} // namespace flux::linux_platform
