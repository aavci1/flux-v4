#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace lambda {

enum class ChatRole {
    User,
    Assistant,
};

struct ChatMessage {
    ChatRole role = ChatRole::Assistant;
    std::string text;

    bool operator==(ChatMessage const &) const = default;
};

struct ChatThread {
    std::string title;
    std::string updatedAt;
    std::vector<ChatMessage> messages;

    bool operator==(ChatThread const &) const = default;
};

inline std::string chatPreview(ChatThread const &thread) {
    if (thread.messages.empty()) {
        return "No messages yet.";
    }
    return thread.messages.back().text;
}

inline std::string shortenForPreview(std::string text, std::size_t maxLength = 88) {
    if (text.size() <= maxLength) {
        return text;
    }
    text.resize(maxLength);
    text += "...";
    return text;
}

inline std::string mockAssistantReply(std::string_view chatTitle, std::string_view userPrompt) {
    std::string summary = std::string(userPrompt);
    if (summary.empty()) {
        summary = "the latest context";
    } else if (summary.size() > 72) {
        summary = shortenForPreview(summary, 72);
    }

    return "Let's keep " + std::string(chatTitle) + " moving. Based on " + summary +
           ", I would break the next step into a small concrete change, verify it, and then iterate.";
}

inline std::vector<ChatThread> sampleChatThreads() {
    return {
        {
            .title = "Launch planning",
            .updatedAt = "4h",
            .messages = {
                {ChatRole::Assistant, "We have the release window locked in. Do you want to focus on the rollout checklist or the announcement copy first?"},
                {ChatRole::User, "Let's start with the rollout checklist."},
                {ChatRole::Assistant, "Perfect. I would split that into QA signoff, docs updates, and post-release monitoring so we can track each part clearly."},
            },
        },
        {
            .title = "Research notes",
            .updatedAt = "9h",
            .messages = {
                {ChatRole::Assistant, "I compared a few local models and wrote down the tradeoffs around latency, RAM, and instruction following."},
                {ChatRole::User, "What should we test next?"},
                {ChatRole::Assistant, "Next I would run the same prompt set against longer context windows so we can see where quality falls off."},
            },
        },
        {
            .title = "Prompt experiments",
            .updatedAt = "1d",
            .messages = {
                {ChatRole::User, "The current system prompt still feels too verbose."},
                {ChatRole::Assistant, "Agreed. We can tighten it by keeping only behavior-critical instructions and moving style examples into tests."},
            },
        },
        {
            .title = "Design review",
            .updatedAt = "2d",
            .messages = {
                {ChatRole::Assistant, "The shell feels promising. The biggest gap now is that the conversation area still reads like a placeholder."},
                {ChatRole::User, "Let's make the chat experience feel more intentional."},
            },
        },
        {
            .title = "Bug triage",
            .updatedAt = "4d",
            .messages = {
                {ChatRole::User, "Sidebar selection now works, but the chat module needs a real conversation view."},
                {ChatRole::Assistant, "That sounds like the right next step. We can add a dedicated ChatView with bubbles, a composer, and parent-owned thread state."},
            },
        },
    };
}

} // namespace lambda
