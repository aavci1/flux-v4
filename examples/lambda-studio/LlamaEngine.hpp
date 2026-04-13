#pragma once

#include "BackendTypes.hpp"

#include "chat.h"
#include "common.h"
#include "sampling.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace lambda_backend {

struct LlmUiEvent {
    enum class Kind {
        Chunk,
        Done,
        Error,
    };
    enum class Part {
        Content,
        Thinking,
    };

    Kind kind = Kind::Done;
    Part part = Part::Content;
    std::string chatId;
    std::string text;
};

namespace detail {

// Post directly to the app callback so the event queue can wake the UI loop from the worker thread.
// Dispatching onto the Cocoa main queue here can stall until the next OS event.
inline void postEventToMain(std::function<void(LlmUiEvent)> const &post, LlmUiEvent ev) {
    post(std::move(ev));
}

inline bool envFlagEnabled(char const *name) {
    char const *value = std::getenv(name);
    return value && value[0] != '\0' && std::strcmp(value, "0") != 0;
}

inline int envIntOr(char const *name, int fallback) {
    char const *value = std::getenv(name);
    if (!value || value[0] == '\0') {
        return fallback;
    }
    return std::max(1, std::atoi(value));
}

} // namespace detail

inline bool debugFakeStreamEnabled() {
    return detail::envFlagEnabled("LAMBDA_STUDIO_FAKE_STREAM");
}

inline bool debugAutoStreamEnabled() {
    return detail::envFlagEnabled("LAMBDA_STUDIO_AUTO_STREAM");
}

inline int debugFakeTokensPerSecond() {
    return detail::envIntOr("LAMBDA_STUDIO_FAKE_TOKENS_PER_SECOND", 10);
}

inline std::string debugFakeMarkdownResponse() {
    return R"(# Streaming Markdown Stress Test

## Overview

This response is intentionally long so we can observe **CPU usage** while the UI receives markdown over time.

- The renderer should keep old content stable.
- Only the newest chunk should be changing.
- Inline `code` and headings are included on purpose.

### Checklist

- Rebuild the attributed markdown only when needed.
- Avoid expensive work for messages that are only being scrolled.
- Keep the chat responsive while the stream is active.

```cpp
for (int frame = 0; frame < 600; ++frame) {
    render_chat_transcript();
    sample_cpu_usage();
}
```

## Notes

The quick brown fox jumps over the lazy dog. The quick brown fox jumps over the lazy dog. The quick brown fox jumps over the lazy dog.

The UI should render **bold emphasis**, keep `inline spans` readable, and avoid chewing CPU once the message is complete.

- Item one explains the cost of parsing.
- Item two explains the cost of text layout.
- Item three explains the cost of rebuilding the whole transcript.

### Closing

If this fake stream still drives high CPU after the response settles, the hotspot is probably outside the markdown parser and closer to layout or transcript rebuilding.)";
}

inline std::string tokenToPiece(llama_vocab const *vocab, llama_token token) {
    char buf[256];
    int const n = llama_token_to_piece(vocab, token, buf, sizeof(buf), 0, true);
    if (n < 0) {
        std::string text(static_cast<std::size_t>(-n), '\0');
        llama_token_to_piece(vocab, token, text.data(), static_cast<int32_t>(text.size()), 0, true);
        return text;
    }
    return std::string(buf, static_cast<std::size_t>(n));
}

class LlamaEngine {
  public:
    LlamaEngine() = default;

    ~LlamaEngine() {
        cancelGeneration();
        joinWorker();
        unload();
    }

    LlamaEngine(LlamaEngine const &) = delete;
    LlamaEngine &operator=(LlamaEngine const &) = delete;

    bool load(std::string const &modelPath, int nGpuLayers = -1, uint32_t nCtx = 0) {
        std::lock_guard<std::mutex> lock(mutex_);
        unloadLocked();

        ggml_backend_load_all();
        llama_backend_init();

        params_ = common_params {};
        params_.model.path = modelPath;
        params_.n_gpu_layers = nGpuLayers < 0 ? 999 : nGpuLayers;
        params_.n_ctx = nCtx;

        auto const modelParams = common_model_params_to_llama(params_);
        model_.reset(llama_model_load_from_file(modelPath.c_str(), modelParams));
        if (!model_) {
            std::fprintf(stderr, "[LlamaEngine] failed to load model: %s\n", modelPath.c_str());
            return false;
        }

        vocab_ = llama_model_get_vocab(model_.get());
        modelPath_ = modelPath;
        templates_ = common_chat_templates_init(model_.get(), "");

        std::fprintf(
            stderr,
            "[LlamaEngine] loaded: %s (n_gpu_layers=%d, n_ctx=%u)\n",
            modelPath.c_str(),
            params_.n_gpu_layers,
            nCtx
        );
        std::fprintf(
            stderr,
            "[LlamaEngine] chat template source: %.120s\n",
            common_chat_templates_source(templates_.get()).c_str()
        );
        return true;
    }

    bool isLoaded() const { return model_ != nullptr; }

    std::string const &modelPath() const { return modelPath_; }

    SamplingParams samplingParams() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return samplingParams_;
    }

    void setSamplingParams(SamplingParams const &params) {
        std::lock_guard<std::mutex> lock(mutex_);
        samplingParams_ = params;
    }

    void unload() {
        std::lock_guard<std::mutex> lock(mutex_);
        unloadLocked();
    }

    void cancelGeneration() { cancelled_.store(true, std::memory_order_relaxed); }

    void startChat(
        std::vector<ChatMessage> messages,
        std::string chatId,
        std::function<void(LlmUiEvent)> post
    ) {
        if (debugFakeStreamEnabled()) {
            cancelGeneration();
            joinWorker();
            cancelled_.store(false, std::memory_order_relaxed);

            worker_ = std::thread(
                [this, msgs = std::move(messages), chat = std::move(chatId), userPost = std::move(post)]() mutable {
                    runFakeGeneration(std::move(msgs), std::move(chat), std::move(userPost));
                }
            );
            return;
        }

        if (!model_) {
            detail::postEventToMain(
                post,
                LlmUiEvent {
                    .kind = LlmUiEvent::Kind::Error,
                    .chatId = chatId,
                    .text = "No model loaded",
                }
            );
            return;
        }

        cancelGeneration();
        joinWorker();
        cancelled_.store(false, std::memory_order_relaxed);

        worker_ = std::thread(
            [this, msgs = std::move(messages), chat = std::move(chatId), userPost = std::move(post)]() mutable {
                runGeneration(std::move(msgs), std::move(chat), std::move(userPost));
            }
        );
    }

  private:
    void joinWorker() {
        if (worker_.joinable()) {
            worker_.join();
        }
    }

    void unloadLocked() {
        cancelGeneration();
        joinWorker();
        templates_.reset();
        vocab_ = nullptr;
        model_.reset();
        modelPath_.clear();
    }

    void runGeneration(
        std::vector<ChatMessage> messages,
        std::string chatId,
        std::function<void(LlmUiEvent)> userPost
    ) {
        auto postOnMain = [&userPost, &chatId](LlmUiEvent ev) {
            ev.chatId = chatId;
            detail::postEventToMain(userPost, std::move(ev));
        };

        std::vector<common_chat_msg> chatMessages;
        chatMessages.reserve(messages.size());
        for (auto const &message : messages) {
            if (message.role == ChatMessage::Role::Reasoning) {
                continue;
            }
            if (message.role == ChatMessage::Role::Assistant && message.text.empty()) {
                continue;
            }
            common_chat_msg chatMessage;
            chatMessage.role = message.role == ChatMessage::Role::User ? "user" : "assistant";
            chatMessage.content = message.text;
            chatMessages.push_back(std::move(chatMessage));
        }
        while (
            !chatMessages.empty() && chatMessages.back().role == "assistant" &&
            chatMessages.back().content.empty()
        ) {
            chatMessages.pop_back();
        }

        common_chat_templates_inputs inputs;
        inputs.messages = chatMessages;
        inputs.add_generation_prompt = true;
        inputs.use_jinja = true;
        inputs.reasoning_format = COMMON_REASONING_FORMAT_DEEPSEEK;
        inputs.enable_thinking = common_chat_templates_support_enable_thinking(templates_.get());

        common_chat_params const chatParams = common_chat_templates_apply(templates_.get(), inputs);

        std::fprintf(
            stderr,
            "[LlamaEngine] template format=%s supports_thinking=%d\n",
            common_chat_format_name(chatParams.format),
            static_cast<int>(chatParams.supports_thinking)
        );
        std::fprintf(stderr, "[LlamaEngine] prompt (first 200 chars): %.200s\n", chatParams.prompt.c_str());

        auto const promptTokens = common_tokenize(vocab_, chatParams.prompt, true, true);
        if (promptTokens.empty()) {
            postOnMain(LlmUiEvent {.kind = LlmUiEvent::Kind::Error, .text = "Empty prompt after tokenization"});
            return;
        }

        SamplingParams params;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            params = samplingParams_;
        }

        int const promptCount = static_cast<int>(promptTokens.size());
        int const predictCount = params.maxTokens;

        std::fprintf(stderr, "[LlamaEngine] prompt: %d tokens, max predict: %d\n", promptCount, predictCount);

        auto contextParams = common_context_params_to_llama(params_);
        llama_context_ptr ctx(llama_init_from_model(model_.get(), contextParams));
        if (!ctx) {
            postOnMain(LlmUiEvent {.kind = LlmUiEvent::Kind::Error, .text = "Failed to create llama context"});
            return;
        }

        common_params_sampling sampling {};
        sampling.temp = params.temp;
        sampling.top_p = params.topP;
        sampling.top_k = params.topK;
        common_sampler_ptr sampler(common_sampler_init(model_.get(), sampling));

        {
            int32_t const batchSize = llama_n_batch(ctx.get());
            for (int32_t i = 0; i < promptCount; i += batchSize) {
                int32_t const tokenCount = std::min(batchSize, promptCount - i);
                auto *batchTokens = const_cast<llama_token *>(promptTokens.data() + i);
                llama_batch const batch = llama_batch_get_one(batchTokens, tokenCount);
                if (llama_decode(ctx.get(), batch) != 0) {
                    postOnMain(
                        LlmUiEvent {.kind = LlmUiEvent::Kind::Error, .text = decodeError("prompt evaluation")}
                    );
                    return;
                }
                if (cancelled_.load(std::memory_order_relaxed)) {
                    postOnMain(LlmUiEvent {.kind = LlmUiEvent::Kind::Done});
                    return;
                }
            }
        }

        common_chat_parser_params parserParams(chatParams);
        parserParams.reasoning_format = COMMON_REASONING_FORMAT_DEEPSEEK;
        if (!chatParams.parser.empty()) {
            parserParams.parser.load(chatParams.parser);
        }

        std::string rawOutput;
        common_chat_msg previousMessage;
        previousMessage.role = "assistant";

        for (int i = 0; i < predictCount; ++i) {
            if (cancelled_.load(std::memory_order_relaxed)) {
                postOnMain(LlmUiEvent {.kind = LlmUiEvent::Kind::Done});
                return;
            }

            llama_token token = common_sampler_sample(sampler.get(), ctx.get(), -1);
            if (llama_vocab_is_eog(vocab_, token)) {
                std::fprintf(stderr, "[LlamaEngine] EOG token=%d at position %d\n", static_cast<int>(token), i);
                break;
            }

            common_sampler_accept(sampler.get(), token, true);

            rawOutput += tokenToPiece(vocab_, token);

            auto parsedMessage = common_chat_parse(rawOutput, true, parserParams);
            auto diffs = common_chat_msg_diff::compute_diffs(previousMessage, parsedMessage);
            for (auto &diff : diffs) {
                if (!diff.reasoning_content_delta.empty()) {
                    postOnMain(
                        LlmUiEvent {
                            .kind = LlmUiEvent::Kind::Chunk,
                            .part = LlmUiEvent::Part::Thinking,
                            .text = std::move(diff.reasoning_content_delta),
                        }
                    );
                }
                if (!diff.content_delta.empty()) {
                    postOnMain(
                        LlmUiEvent {
                            .kind = LlmUiEvent::Kind::Chunk,
                            .part = LlmUiEvent::Part::Content,
                            .text = std::move(diff.content_delta),
                        }
                    );
                }
            }
            previousMessage = std::move(parsedMessage);

            llama_batch const generationBatch = llama_batch_get_one(&token, 1);
            if (llama_decode(ctx.get(), generationBatch) != 0) {
                postOnMain(LlmUiEvent {.kind = LlmUiEvent::Kind::Error, .text = decodeError("generation")});
                return;
            }
        }

        std::fprintf(
            stderr,
            "[LlamaEngine] generation done. raw output (first 500 chars):\n%.500s\n[END]\n",
            rawOutput.c_str()
        );

        postOnMain(LlmUiEvent {.kind = LlmUiEvent::Kind::Done});
    }

    void runFakeGeneration(
        std::vector<ChatMessage> messages,
        std::string chatId,
        std::function<void(LlmUiEvent)> userPost
    ) {
        (void)messages;
        auto postOnMain = [&userPost, &chatId](LlmUiEvent ev) {
            ev.chatId = chatId;
            detail::postEventToMain(userPost, std::move(ev));
        };

        std::vector<std::pair<LlmUiEvent::Part, std::string>> tokens;
        auto tokenize = [&tokens](LlmUiEvent::Part part, std::string const &text) {
            std::size_t index = 0;
            while (index < text.size()) {
                if (text[index] == '\n') {
                    tokens.push_back({part, "\n"});
                    ++index;
                    continue;
                }

                std::size_t end = index;
                while (end < text.size() && text[end] != ' ' && text[end] != '\n') {
                    ++end;
                }
                if (end < text.size() && text[end] == ' ') {
                    ++end;
                }
                tokens.push_back({part, text.substr(index, end - index)});
                index = end;
            }
        };

        tokenize(
            LlmUiEvent::Part::Thinking,
            "Thinking through the markdown rendering path, checking headings, bold markers, and code fences.\n\n"
        );
        tokenize(LlmUiEvent::Part::Content, debugFakeMarkdownResponse());

        auto const delay = std::chrono::milliseconds(std::max(1, 1000 / debugFakeTokensPerSecond()));
        for (auto const &[part, text] : tokens) {
            if (cancelled_.load(std::memory_order_relaxed)) {
                postOnMain(LlmUiEvent {.kind = LlmUiEvent::Kind::Done});
                return;
            }

            postOnMain(LlmUiEvent {
                .kind = LlmUiEvent::Kind::Chunk,
                .part = part,
                .text = text,
            });
            std::this_thread::sleep_for(delay);
        }

        postOnMain(LlmUiEvent {.kind = LlmUiEvent::Kind::Done});
    }

    std::string decodeError(char const *phase) const {
        std::string message = std::string("Decode failed during ") + phase + ".";
        if (params_.n_gpu_layers != 0) {
            message += " If you see a GPU Hang in the logs, re-run with LLAMA_N_GPU_LAYERS=0 to use CPU-only inference.";
        }
        return message;
    }

    mutable std::mutex mutex_;
    common_params params_;
    llama_model_ptr model_;
    common_chat_templates_ptr templates_;
    llama_vocab const *vocab_ = nullptr;
    std::string modelPath_;
    SamplingParams samplingParams_;
    std::atomic<bool> cancelled_ {false};
    std::thread worker_;
};

inline std::string defaultModelPath() {
    if (char const *path = std::getenv("LLAMA_MODEL_PATH")) {
        return std::string(path);
    }
    return {};
}

inline std::string defaultModelName() {
    if (char const *name = std::getenv("LLAMA_MODEL_NAME")) {
        return std::string(name);
    }
    return "local";
}

inline int defaultNGpuLayers() {
    if (char const *value = std::getenv("LLAMA_N_GPU_LAYERS")) {
        return std::atoi(value);
    }
    return -1;
}

} // namespace lambda_backend
