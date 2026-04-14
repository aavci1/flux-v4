#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace lambda {

inline std::uint64_t generateMessageRenderKey() {
    static std::atomic<std::uint64_t> nextKey {1};
    return nextKey.fetch_add(1, std::memory_order_relaxed);
}

inline std::uint64_t generateGenerationId() {
    static std::atomic<std::uint64_t> nextId {1};
    return nextId.fetch_add(1, std::memory_order_relaxed);
}

enum class ChatRole {
    User,
    Reasoning,
    Assistant,
};

struct MessageGenerationStats {
    std::string modelPath;
    std::string modelName;
    std::int64_t promptTokens = 0;
    std::int64_t completionTokens = 0;
    std::int64_t startedAtUnixMs = 0;
    std::int64_t firstTokenAtUnixMs = 0;
    std::int64_t finishedAtUnixMs = 0;
    double tokensPerSecond = 0.0;
    std::string status;
    std::string errorText;
    float temp = 0.f;
    float topP = 0.f;
    std::int32_t topK = 0;
    std::int32_t maxTokens = 0;

    bool operator==(MessageGenerationStats const &) const = default;
};

struct ChatMessage {
    struct Paragraph {
        std::string text;
        std::uint64_t renderKey = generateMessageRenderKey();
        std::uint64_t textRevision = 1;

        bool operator==(Paragraph const &) const = default;
    };

    ChatRole role = ChatRole::Assistant;
    std::string text;
    std::vector<Paragraph> paragraphs;
    std::int64_t startedAtNanos = 0;
    std::int64_t finishedAtNanos = 0;
    bool collapsed = false;
    std::optional<MessageGenerationStats> generationStats;
    std::uint64_t renderKey = generateMessageRenderKey();
    std::uint64_t textRevision = 1;

    bool operator==(ChatMessage const &) const = default;
};

struct ChatThread {
    std::string id;
    std::string title;
    std::int64_t updatedAtUnixMs = 0;
    std::string modelPath;
    std::string modelName;
    std::vector<ChatMessage> messages;
    std::vector<ChatMessage> streamDraftMessages;
    bool streaming = false;
    std::uint64_t activeGenerationId = 0;

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

inline std::int64_t currentUnixMillis() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

inline std::string chatUpdatedAtLabel(ChatThread const &thread, std::int64_t nowUnixMs = currentUnixMillis()) {
    if (thread.updatedAtUnixMs <= 0) {
        return {};
    }

    std::int64_t const deltaMs = std::max<std::int64_t>(0, nowUnixMs - thread.updatedAtUnixMs);
    constexpr std::int64_t kMinuteMs = 60 * 1000;
    constexpr std::int64_t kHourMs = 60 * kMinuteMs;
    constexpr std::int64_t kDayMs = 24 * kHourMs;
    constexpr std::int64_t kWeekMs = 7 * kDayMs;

    if (deltaMs < kMinuteMs) {
        return "now";
    }
    if (deltaMs < kHourMs) {
        return std::to_string(deltaMs / kMinuteMs) + "m";
    }
    if (deltaMs < kDayMs) {
        return std::to_string(deltaMs / kHourMs) + "h";
    }
    if (deltaMs < kWeekMs) {
        return std::to_string(deltaMs / kDayMs) + "d";
    }
    return std::to_string(deltaMs / kWeekMs) + "w";
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

inline bool markdownLineIsBlank(std::string_view line) {
    return std::all_of(line.begin(), line.end(), [](char ch) {
        return ch == ' ' || ch == '\t' || ch == '\r';
    });
}

inline bool markdownLineTogglesFence(std::string_view line) {
    std::size_t index = 0;
    while (index < line.size() && line[index] == ' ') {
        ++index;
    }
    return index + 3 <= line.size() && line.substr(index, 3) == "```";
}

inline std::vector<std::string> splitMarkdownParagraphs(std::string_view text) {
    std::vector<std::string> paragraphs;
    if (text.empty()) {
        return paragraphs;
    }

    std::string current;
    bool inFence = false;
    std::size_t lineStart = 0;

    auto flushCurrent = [&]() {
        if (!current.empty()) {
            paragraphs.push_back(current);
            current.clear();
        }
    };

    while (lineStart < text.size()) {
        std::size_t lineEnd = lineStart;
        while (lineEnd < text.size() && text[lineEnd] != '\n') {
            ++lineEnd;
        }

        bool const endsNewline = lineEnd < text.size() && text[lineEnd] == '\n';
        std::string_view const line = text.substr(lineStart, lineEnd - lineStart);
        bool const blankLine = markdownLineIsBlank(line);

        if (!inFence && blankLine) {
            flushCurrent();
            lineStart = endsNewline ? lineEnd + 1 : lineEnd;
            continue;
        }

        if (!current.empty()) {
            current.push_back('\n');
        }
        current.append(line.data(), line.size());

        if (markdownLineTogglesFence(line)) {
            inFence = !inFence;
        }

        lineStart = endsNewline ? lineEnd + 1 : lineEnd;
    }

    flushCurrent();
    return paragraphs;
}

inline void syncAssistantParagraphs(ChatMessage &message) {
    if (message.role != ChatRole::Assistant) {
        message.paragraphs.clear();
        return;
    }

    std::vector<std::string> const nextParagraphs = splitMarkdownParagraphs(message.text);
    std::size_t const sharedCount = std::min(message.paragraphs.size(), nextParagraphs.size());
    for (std::size_t i = 0; i < sharedCount; ++i) {
        if (message.paragraphs[i].text == nextParagraphs[i]) {
            continue;
        }
        message.paragraphs[i].text = nextParagraphs[i];
        ++message.paragraphs[i].textRevision;
    }

    if (message.paragraphs.size() > nextParagraphs.size()) {
        message.paragraphs.resize(nextParagraphs.size());
    } else {
        message.paragraphs.reserve(nextParagraphs.size());
        for (std::size_t i = message.paragraphs.size(); i < nextParagraphs.size(); ++i) {
            message.paragraphs.push_back(ChatMessage::Paragraph {
                .text = nextParagraphs[i],
            });
        }
    }
}

inline void appendChatMessageText(ChatMessage &message, std::string_view delta) {
    if (delta.empty()) {
        return;
    }
    message.text.append(delta.data(), delta.size());
    ++message.textRevision;
    syncAssistantParagraphs(message);
}

inline void syncChatThreadParagraphs(ChatThread &thread) {
    for (ChatMessage &message : thread.messages) {
        syncAssistantParagraphs(message);
    }
    for (ChatMessage &message : thread.streamDraftMessages) {
        syncAssistantParagraphs(message);
    }
}

inline std::vector<ChatThread> sampleChatThreads() {
    std::int64_t const now = currentUnixMillis();
    std::vector<ChatThread> threads = {
        {
            .id = generateChatId(),
            .title = "Launch planning",
            .updatedAtUnixMs = now - 4LL * 60 * 60 * 1000,
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
            .updatedAtUnixMs = now - 9LL * 60 * 60 * 1000,
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
            .updatedAtUnixMs = now - 24LL * 60 * 60 * 1000,
            .messages = {
                {ChatRole::User, "The current system prompt still feels too verbose."},
                {ChatRole::Assistant, "Agreed. We can tighten it by keeping only behavior-critical instructions and moving style examples into tests."},
            },
            .streaming = false,
        },
        {
            .id = generateChatId(),
            .title = "Design review",
            .updatedAtUnixMs = now - 2LL * 24 * 60 * 60 * 1000,
            .messages = {
                {ChatRole::Assistant, "The shell feels promising. The biggest gap now is that the conversation area still reads like a placeholder."},
                {ChatRole::User, "Let's make the chat experience feel more intentional."},
            },
            .streaming = false,
        },
        {
            .id = generateChatId(),
            .title = "Bug triage",
            .updatedAtUnixMs = now - 4LL * 24 * 60 * 60 * 1000,
            .messages = {
                {ChatRole::User, "Sidebar selection now works, but the chat module needs a real conversation view."},
                {ChatRole::Assistant, "That sounds like the right next step. We can add a dedicated ChatView with bubbles, a composer, and parent-owned thread state."},
            },
            .streaming = false,
        },
    };
    for (ChatThread &thread : threads) {
        syncChatThreadParagraphs(thread);
    }
    return threads;
}

} // namespace lambda
