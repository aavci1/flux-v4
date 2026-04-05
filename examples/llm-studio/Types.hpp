#pragma once

#include <array>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>
#include <vector>

/// RFC 4122 UUID version 4 (random), lowercase hex with dashes.
inline std::string generateChatId() {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<unsigned> byteDis(0, 255);
  std::array<unsigned char, 16> bytes {};
  for (auto& b : bytes) {
    b = static_cast<unsigned char>(byteDis(gen));
  }
  bytes[6] = static_cast<unsigned char>((bytes[6] & 0x0fu) | 0x40u);
  bytes[8] = static_cast<unsigned char>((bytes[8] & 0x3fu) | 0x80u);

  std::ostringstream oss;
  oss << std::hex << std::setfill('0');
  for (size_t i = 0; i < 16; ++i) {
    oss << std::setw(2) << static_cast<unsigned>(bytes[i]);
    if (i == 3 || i == 5 || i == 7 || i == 9) {
      oss << '-';
    }
  }
  return oss.str();
}

struct ChatMessage {
    enum class Role { User, Reasoning, Assistant };

    Role role = Role::User;
    std::string text;

    constexpr bool operator==(ChatMessage const& o) const = default;
};

struct Chat {
    std::string id {};
    std::string title {""};
    std::vector<ChatMessage> messages {};
    bool streaming = false;

    constexpr bool operator==(Chat const& o) const = default;
};