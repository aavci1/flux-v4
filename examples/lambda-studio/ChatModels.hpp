#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace lambda {

enum class ChatRole {
    User,
    Reasoning,
    Assistant,
};

struct ChatMessage {
    ChatRole role = ChatRole::Assistant;
    std::string text;
    std::int64_t startedAtNanos = 0;
    std::int64_t finishedAtNanos = 0;
    bool collapsed = false;

    bool operator==(ChatMessage const &) const = default;
};

struct ChatThread {
    std::string id;
    std::string title;
    std::string updatedAt;
    std::string modelPath;
    std::string modelName;
    std::vector<ChatMessage> messages;
    bool streaming = false;

    bool operator==(ChatThread const &) const = default;
};

inline std::string generateChatId() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<unsigned> byteDis(0, 255);
    std::array<unsigned char, 16> bytes {};
    for (auto &b : bytes) {
        b = static_cast<unsigned char>(byteDis(gen));
    }
    bytes[6] = static_cast<unsigned char>((bytes[6] & 0x0fu) | 0x40u);
    bytes[8] = static_cast<unsigned char>((bytes[8] & 0x3fu) | 0x80u);

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < 16; ++i) {
        oss << std::setw(2) << static_cast<unsigned>(bytes[i]);
        if (i == 3 || i == 5 || i == 7 || i == 9) {
            oss << '-';
        }
    }
    return oss.str();
}

inline std::string chatPreview(ChatThread const &thread) {
    for (auto it = thread.messages.rbegin(); it != thread.messages.rend(); ++it) {
        if (it->role == ChatRole::Reasoning || it->text.empty()) {
            continue;
        }
        return it->text;
    }
    if (thread.messages.empty()) {
        return "No messages yet.";
    }
    return "Waiting for the next response...";
}

inline std::string shortenForPreview(std::string text, std::size_t maxLength = 88) {
    if (text.size() <= maxLength) {
        return text;
    }
    text.resize(maxLength);
    text += "...";
    return text;
}

inline std::vector<ChatThread> sampleChatThreads() {
    return {
        {
            .id = generateChatId(),
            .title = "Launch planning",
            .updatedAt = "4h",
            .messages = {
                {ChatRole::Assistant, "We have the release window locked in. Do you want to focus on the rollout checklist or the announcement copy first?"},
                {ChatRole::User, "Let's start with the rollout checklist."},
                {ChatRole::Assistant, "Perfect. I would split that into QA signoff, docs updates, and post-release monitoring so we can track each part clearly."},
            },
            .streaming = false,
        },
        {
            .id = generateChatId(),
            .title = "Research notes",
            .updatedAt = "9h",
            .messages = {
                {ChatRole::Assistant, "I compared a few local models and wrote down the tradeoffs around latency, RAM, and instruction following."},
                {ChatRole::User, "What should we test next?"},
                {ChatRole::Assistant, "Next I would run the same prompt set against longer context windows so we can see where quality falls off."},
            },
            .streaming = false,
        },
        {
            .id = generateChatId(),
            .title = "Prompt experiments",
            .updatedAt = "1d",
            .messages = {
                {ChatRole::User, "The current system prompt still feels too verbose."},
                {ChatRole::Assistant, "Agreed. We can tighten it by keeping only behavior-critical instructions and moving style examples into tests."},
            },
            .streaming = false,
        },
        {
            .id = generateChatId(),
            .title = "Design review",
            .updatedAt = "2d",
            .messages = {
                {ChatRole::Assistant, "The shell feels promising. The biggest gap now is that the conversation area still reads like a placeholder."},
                {ChatRole::User, "Let's make the chat experience feel more intentional."},
            },
            .streaming = false,
        },
        {
            .id = generateChatId(),
            .title = "Bug triage",
            .updatedAt = "4d",
            .messages = {
                {ChatRole::User, "Sidebar selection now works, but the chat module needs a real conversation view."},
                {ChatRole::Assistant, "That sounds like the right next step. We can add a dedicated ChatView with bubbles, a composer, and parent-owned thread state."},
            },
            .streaming = false,
        },
    };
}

} // namespace lambda
