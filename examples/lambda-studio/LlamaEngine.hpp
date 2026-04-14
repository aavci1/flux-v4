#pragma once

#include "Debug.hpp"
#include "Defaults.hpp"
#include "Interfaces.hpp"
#include "Types.hpp"

#include "chat.h"
#include "common.h"
#include "sampling.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace lambda_studio_backend {

namespace detail {

// Post directly to the app callback so the event queue can wake the UI loop from the worker thread.
// Dispatching onto the Cocoa main queue here can stall until the next OS event.
inline void postEventToMain(std::function<void(LlmUiEvent)> const &post, LlmUiEvent ev) {
    post(std::move(ev));
}

class UiChunkBatcher {
  public:
    explicit UiChunkBatcher(std::function<void(LlmUiEvent)> post) : post_(std::move(post)) {}

    void push(LlmUiEvent::Part part, std::string text) {
        if (text.empty()) {
            return;
        }

        auto const now = std::chrono::steady_clock::now();
        if (hasPending_ && pendingPart_ != part) {
            flush();
        }
        if (!hasPending_) {
            pendingPart_ = part;
            hasPending_ = true;
            pendingSince_ = now;
        }

        pendingText_ += std::move(text);

        auto const elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - pendingSince_);
        bool const largeEnough = pendingText_.size() >= 192;
        bool const staleEnough = elapsed.count() >= 250;
        bool const paragraphBoundary = pendingText_.find("\n\n") != std::string::npos;
        if (largeEnough || staleEnough || paragraphBoundary) {
            flush();
        }
    }

    void flush() {
        if (!hasPending_ || pendingText_.empty()) {
            pendingText_.clear();
            hasPending_ = false;
            return;
        }

        post_(LlmUiEvent {
            .kind = LlmUiEvent::Kind::Chunk,
            .part = pendingPart_,
            .text = std::move(pendingText_),
        });
        pendingText_.clear();
        hasPending_ = false;
    }

  private:
    std::function<void(LlmUiEvent)> post_;
    LlmUiEvent::Part pendingPart_ = LlmUiEvent::Part::Content;
    std::string pendingText_;
    std::chrono::steady_clock::time_point pendingSince_ {};
    bool hasPending_ = false;
};

class StreamingParseAccumulator {
  public:
    explicit StreamingParseAccumulator(
        common_chat_parser_params parserParams,
        UiChunkBatcher &batcher
    )
        : parserParams_(std::move(parserParams)), batcher_(batcher) {
        previousMessage_.role = "assistant";
    }

    void append(std::string piece) {
        if (piece.empty()) {
            return;
        }

        bool const shouldFlush = piece.find('\n') != std::string::npos || tokensSinceFlush_ >= 15;
        rawOutput_ += std::move(piece);
        ++tokensSinceFlush_;
        dirty_ = true;
        if (shouldFlush) {
            flush();
        }
    }

    void flush() {
        if (!dirty_) {
            return;
        }

        auto parsedMessage = common_chat_parse(rawOutput_, true, parserParams_);
        auto diffs = common_chat_msg_diff::compute_diffs(previousMessage_, parsedMessage);
        for (auto &diff : diffs) {
            if (!diff.reasoning_content_delta.empty()) {
                batcher_.push(LlmUiEvent::Part::Thinking, std::move(diff.reasoning_content_delta));
            }
            if (!diff.content_delta.empty()) {
                batcher_.push(LlmUiEvent::Part::Content, std::move(diff.content_delta));
            }
        }
        previousMessage_ = std::move(parsedMessage);
        tokensSinceFlush_ = 0;
        dirty_ = false;
    }

    std::string const &rawOutput() const { return rawOutput_; }
    common_chat_msg const &parsedMessage() const { return previousMessage_; }

  private:
    common_chat_parser_params parserParams_;
    UiChunkBatcher &batcher_;
    std::string rawOutput_;
    common_chat_msg previousMessage_;
    int tokensSinceFlush_ = 0;
    bool dirty_ = false;
};

} // namespace detail

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

inline std::int64_t currentUnixMillis() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

inline double computeTokensPerSecond(GenerationStats const &stats) {
    if (stats.completionTokens <= 0) {
        return 0.0;
    }
    std::int64_t const startMs = stats.firstTokenAtUnixMs > 0 ? stats.firstTokenAtUnixMs : stats.startedAtUnixMs;
    std::int64_t const durationMs = std::max<std::int64_t>(1, stats.finishedAtUnixMs - startMs);
    return static_cast<double>(stats.completionTokens) / (static_cast<double>(durationMs) / 1000.0);
}

class LlamaEngine : public lambda::IChatEngine {
  public:
    LlamaEngine() = default;

    ~LlamaEngine() {
        cancelAllGenerations();
        unload();
    }

    LlamaEngine(LlamaEngine const &) = delete;
    LlamaEngine &operator=(LlamaEngine const &) = delete;

    bool load(std::string const &modelPath, int nGpuLayers = -1, uint32_t nCtx = 0) override {
        cancelAllGenerations();

        ggml_backend_load_all();
        llama_backend_init();

        common_params params {};
        params.model.path = modelPath;
        params.n_gpu_layers = nGpuLayers < 0 ? 999 : nGpuLayers;
        params.n_ctx = nCtx;

        auto nextState = std::make_shared<LoadedModelState>();
        nextState->params = params;
        nextState->contextParams = common_context_params_to_llama(params);

        auto const modelParams = common_model_params_to_llama(params);
        nextState->model.reset(llama_model_load_from_file(modelPath.c_str(), modelParams));
        if (!nextState->model) {
            std::fprintf(stderr, "[LlamaEngine] failed to load model: %s\n", modelPath.c_str());
            return false;
        }

        nextState->vocab = llama_model_get_vocab(nextState->model.get());
        nextState->templates = common_chat_templates_init(nextState->model.get(), "");

        std::fprintf(
            stderr,
            "[LlamaEngine] loaded: %s (n_gpu_layers=%d, n_ctx=%u)\n",
            modelPath.c_str(),
            params.n_gpu_layers,
            nCtx
        );
        std::fprintf(
            stderr,
            "[LlamaEngine] chat template source: %.120s\n",
            common_chat_templates_source(nextState->templates.get()).c_str()
        );

        std::lock_guard<std::mutex> lock(mutex_);
        params_ = params;
        modelState_ = std::move(nextState);
        modelPath_ = modelPath;
        return true;
    }

    bool isLoaded() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return modelState_ != nullptr;
    }

    std::string const &modelPath() const override { return modelPath_; }

    SamplingParams samplingParams() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return samplingParams_;
    }

    void setSamplingParams(SamplingParams const &params) override {
        std::lock_guard<std::mutex> lock(mutex_);
        samplingParams_ = params;
    }

    void unload() override {
        cancelAllGenerations();
        std::lock_guard<std::mutex> lock(mutex_);
        modelState_.reset();
        modelPath_.clear();
        params_ = common_params {};
    }

    void cancelChat(std::string const &chatId) override {
        std::shared_ptr<ChatSession> session = findSession(chatId);
        if (!session) {
            return;
        }
        session->cancelled.store(true, std::memory_order_relaxed);
    }

    void cancelAllGenerations() override {
        std::vector<std::shared_ptr<ChatSession>> sessions = snapshotSessions();
        for (auto const &session : sessions) {
            session->cancelled.store(true, std::memory_order_relaxed);
        }
        for (auto const &session : sessions) {
            joinSessionWorker(session);
        }
    }

    void startChat(
        ChatGenerationRequest request,
        std::function<void(LlmUiEvent)> post
    ) override {
        std::shared_ptr<const LoadedModelState> modelState = currentModelState();
        if (!debugFakeStreamEnabled() && !modelState) {
            detail::postEventToMain(
                post,
                LlmUiEvent {
                    .kind = LlmUiEvent::Kind::Error,
                    .chatId = request.chatId,
                    .generationId = request.generationId,
                    .text = "No model loaded",
                }
            );
            return;
        }

        std::shared_ptr<ChatSession> session = getOrCreateSession(request.chatId);
        cancelAndJoinSession(session);
        session->cancelled.store(false, std::memory_order_relaxed);

        std::thread worker(
            [this, session, request = std::move(request), userPost = std::move(post), modelState]() mutable {
                if (debugFakeStreamEnabled()) {
                    runFakeGeneration(session, std::move(request), std::move(userPost));
                    return;
                }
                runGeneration(session, std::move(request), std::move(modelState), std::move(userPost));
            }
        );
        {
            std::lock_guard<std::mutex> lock(session->mutex);
            session->worker = std::move(worker);
        }
    }

  private:
    struct LoadedModelState {
        common_params params;
        llama_model_ptr model;
        common_chat_templates_ptr templates;
        llama_vocab const *vocab = nullptr;
        llama_context_params contextParams {};
    };

    struct ChatSession {
        std::mutex mutex;
        std::thread worker;
        std::atomic<bool> cancelled {false};
        llama_context_ptr ctx;
        common_sampler_ptr sampler;
        std::shared_ptr<const LoadedModelState> modelState;
        std::vector<llama_token> committedTokens;
        std::size_t kvTokens = 0;
        std::string lastSummaryText;
        std::size_t lastSummaryMessageCount = 0;
        std::int64_t lastSummaryUpdatedAtUnixMs = 0;
    };

    struct RenderedPrompt {
        std::vector<common_chat_msg> chatMessages;
        common_chat_params chatParams;
        std::vector<llama_token> promptTokens;
    };

    std::string decodeError(char const *phase) const {
        std::string message = std::string("Decode failed during ") + phase + ".";
        if (params_.n_gpu_layers != 0) {
            message += " If you see a GPU Hang in the logs, re-run with LLAMA_N_GPU_LAYERS=0 to use CPU-only inference.";
        }
        return message;
    }

    SamplingParams currentSamplingParams() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return samplingParams_;
    }

    std::shared_ptr<const LoadedModelState> currentModelState() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return modelState_;
    }

    std::shared_ptr<ChatSession> getOrCreateSession(std::string const &chatId) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sessions_.find(chatId);
        if (it != sessions_.end()) {
            return it->second;
        }
        auto session = std::make_shared<ChatSession>();
        sessions_.emplace(chatId, session);
        return session;
    }

    std::shared_ptr<ChatSession> findSession(std::string const &chatId) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sessions_.find(chatId);
        return it == sessions_.end() ? nullptr : it->second;
    }

    std::vector<std::shared_ptr<ChatSession>> snapshotSessions() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::shared_ptr<ChatSession>> sessions;
        sessions.reserve(sessions_.size());
        for (auto const &[_, session] : sessions_) {
            sessions.push_back(session);
        }
        return sessions;
    }

    static void joinSessionWorker(std::shared_ptr<ChatSession> const &session) {
        std::thread worker;
        {
            std::lock_guard<std::mutex> lock(session->mutex);
            if (!session->worker.joinable()) {
                return;
            }
            worker = std::move(session->worker);
        }
        if (worker.joinable()) {
            worker.join();
        }
    }

    static void cancelAndJoinSession(std::shared_ptr<ChatSession> const &session) {
        session->cancelled.store(true, std::memory_order_relaxed);
        joinSessionWorker(session);
    }

    static std::size_t longestCommonPrefix(
        std::vector<llama_token> const &lhs,
        std::vector<llama_token> const &rhs
    ) {
        std::size_t common = 0;
        std::size_t const limit = std::min(lhs.size(), rhs.size());
        while (common < limit && lhs[common] == rhs[common]) {
            ++common;
        }
        return common;
    }

    static std::string summarySystemMessage(std::string const &summaryText, std::size_t summaryMessageCount) {
        if (summaryText.empty() || summaryMessageCount == 0) {
            return {};
        }
        return "Conversation summary replacing the first " + std::to_string(summaryMessageCount) +
               " visible messages:\n" + summaryText;
    }

    static std::vector<common_chat_msg> buildCommonChatMessages(ChatGenerationRequest const &request) {
        std::vector<common_chat_msg> chatMessages;
        chatMessages.reserve(request.messages.size() + (request.summaryText.empty() ? 0 : 1));
        if (!request.summaryText.empty() && request.summaryMessageCount > 0) {
            common_chat_msg summaryMessage;
            summaryMessage.role = "system";
            summaryMessage.content = summarySystemMessage(request.summaryText, request.summaryMessageCount);
            chatMessages.push_back(std::move(summaryMessage));
        }
        std::size_t const startIndex = std::min(request.summaryMessageCount, request.messages.size());
        for (std::size_t i = startIndex; i < request.messages.size(); ++i) {
            ChatMessage const &message = request.messages[i];
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
        while (!chatMessages.empty() && chatMessages.back().role == "assistant" && chatMessages.back().content.empty()) {
            chatMessages.pop_back();
        }
        return chatMessages;
    }

    static common_chat_templates_inputs makeTemplateInputs(
        LoadedModelState const &modelState,
        std::vector<common_chat_msg> chatMessages,
        bool addGenerationPrompt
    ) {
        common_chat_templates_inputs inputs;
        inputs.messages = std::move(chatMessages);
        inputs.add_generation_prompt = addGenerationPrompt;
        inputs.use_jinja = true;
        inputs.reasoning_format = COMMON_REASONING_FORMAT_DEEPSEEK;
        inputs.enable_thinking = common_chat_templates_support_enable_thinking(modelState.templates.get());
        return inputs;
    }

    static RenderedPrompt renderPrompt(
        LoadedModelState const &modelState,
        ChatGenerationRequest const &request,
        bool addGenerationPrompt
    ) {
        RenderedPrompt rendered;
        rendered.chatMessages = buildCommonChatMessages(request);
        rendered.chatParams = common_chat_templates_apply(
            modelState.templates.get(),
            makeTemplateInputs(modelState, rendered.chatMessages, addGenerationPrompt)
        );
        rendered.promptTokens = common_tokenize(modelState.vocab, rendered.chatParams.prompt, true, true);
        return rendered;
    }

    static std::string roleLabel(ChatMessage::Role role) {
        switch (role) {
        case ChatMessage::Role::User:
            return "User";
        case ChatMessage::Role::Assistant:
            return "Assistant";
        case ChatMessage::Role::Reasoning:
            return "Reasoning";
        }
        return "Assistant";
    }

    static std::string formatMessagesForSummary(
        std::vector<ChatMessage> const &messages,
        std::size_t startIndex,
        std::size_t endIndex
    ) {
        std::ostringstream oss;
        for (std::size_t i = startIndex; i < endIndex; ++i) {
            ChatMessage const &message = messages[i];
            if (message.role == ChatMessage::Role::Reasoning || message.text.empty()) {
                continue;
            }
            oss << roleLabel(message.role) << ": " << message.text << "\n\n";
        }
        return oss.str();
    }

    static std::vector<common_chat_msg> buildSummaryMessages(
        std::string const &existingSummary,
        std::vector<ChatMessage> const &messages,
        std::size_t startIndex,
        std::size_t endIndex
    ) {
        std::vector<common_chat_msg> summaryMessages;
        summaryMessages.reserve(2);

        common_chat_msg systemMessage;
        systemMessage.role = "system";
        systemMessage.content =
            "You maintain a hidden conversation summary for context compaction. "
            "Preserve user requests, decisions, constraints, facts, and unresolved follow-ups. "
            "Return plain text only.";
        summaryMessages.push_back(std::move(systemMessage));

        common_chat_msg userMessage;
        userMessage.role = "user";
        userMessage.content = "Existing summary:\n" + (existingSummary.empty() ? std::string("(none)") : existingSummary) +
                              "\n\nAdditional conversation turns to fold in:\n" +
                              formatMessagesForSummary(messages, startIndex, endIndex) +
                              "\nUpdated summary:";
        summaryMessages.push_back(std::move(userMessage));
        return summaryMessages;
    }

    std::optional<std::string> generateSummary(
        std::shared_ptr<const LoadedModelState> const &modelState,
        std::vector<ChatMessage> const &messages,
        std::size_t startIndex,
        std::size_t endIndex,
        std::string const &existingSummary,
        std::function<bool()> const &isCancelled
    ) const {
        std::vector<common_chat_msg> summaryMessages = buildSummaryMessages(existingSummary, messages, startIndex, endIndex);
        common_chat_params const chatParams = common_chat_templates_apply(
            modelState->templates.get(),
            makeTemplateInputs(*modelState, std::move(summaryMessages), true)
        );
        auto const promptTokens = common_tokenize(modelState->vocab, chatParams.prompt, true, true);
        if (promptTokens.empty()) {
            return std::string(existingSummary);
        }

        llama_context_ptr ctx(llama_init_from_model(modelState->model.get(), modelState->contextParams));
        if (!ctx) {
            return std::nullopt;
        }

        common_params_sampling sampling {};
        sampling.temp = 0.0f;
        sampling.top_p = 1.0f;
        sampling.top_k = 1;
        common_sampler_ptr sampler(common_sampler_init(modelState->model.get(), sampling));
        if (!sampler) {
            return std::nullopt;
        }

        if (!decodeTokens(ctx.get(), promptTokens, 0, isCancelled)) {
            return std::nullopt;
        }

        std::string rawOutput;
        for (int i = 0; i < 256; ++i) {
            if (isCancelled()) {
                return std::nullopt;
            }

            llama_token token = common_sampler_sample(sampler.get(), ctx.get(), -1);
            if (llama_vocab_is_eog(modelState->vocab, token)) {
                break;
            }

            common_sampler_accept(sampler.get(), token, true);
            rawOutput += tokenToPiece(modelState->vocab, token);

            llama_batch const batch = llama_batch_get_one(&token, 1);
            if (llama_decode(ctx.get(), batch) != 0) {
                return std::nullopt;
            }
        }

        common_chat_parser_params parserParams(chatParams);
        parserParams.reasoning_format = COMMON_REASONING_FORMAT_DEEPSEEK;
        if (!chatParams.parser.empty()) {
            parserParams.parser.load(chatParams.parser);
        }
        auto parsedMessage = common_chat_parse(rawOutput, true, parserParams);
        return parsedMessage.content.empty() ? std::optional<std::string>(rawOutput) :
                                               std::optional<std::string>(parsedMessage.content);
    }

    std::size_t maxSummarizableMessageCount(ChatGenerationRequest const &request) const {
        if (request.messages.empty()) {
            return request.summaryMessageCount;
        }

        std::size_t protectedStart = request.messages.size();
        for (std::size_t i = request.messages.size(); i > 0; --i) {
            if (request.messages[i - 1].role != ChatMessage::Role::User) {
                continue;
            }
            protectedStart = i - 1;
            if (protectedStart > 0 && request.messages[protectedStart - 1].role == ChatMessage::Role::Assistant) {
                --protectedStart;
            }
            break;
        }
        return std::max(request.summaryMessageCount, protectedStart);
    }

    std::size_t pickSummaryTargetEnd(
        std::shared_ptr<const LoadedModelState> const &modelState,
        ChatGenerationRequest const &request,
        std::size_t maxEnd,
        std::int32_t tokenBudget
    ) const {
        std::size_t best = request.summaryMessageCount;
        for (std::size_t end = request.summaryMessageCount + 1; end <= maxEnd; ++end) {
            std::vector<common_chat_msg> summaryMessages =
                buildSummaryMessages(request.summaryText, request.messages, request.summaryMessageCount, end);
            common_chat_params const chatParams = common_chat_templates_apply(
                modelState->templates.get(),
                makeTemplateInputs(*modelState, std::move(summaryMessages), true)
            );
            auto const summaryTokens = common_tokenize(modelState->vocab, chatParams.prompt, true, true);
            if (static_cast<std::int32_t>(summaryTokens.size()) > tokenBudget) {
                break;
            }
            best = end;
        }
        return best == request.summaryMessageCount ? request.summaryMessageCount + 1 : best;
    }

    bool ensureSessionContext(
        std::shared_ptr<ChatSession> const &session,
        std::shared_ptr<const LoadedModelState> const &modelState,
        SamplingParams const &samplingParams
    ) const {
        if (session->ctx && session->sampler && session->modelState == modelState) {
            return true;
        }

        session->ctx.reset(llama_init_from_model(modelState->model.get(), modelState->contextParams));
        if (!session->ctx) {
            return false;
        }

        common_params_sampling sampling {};
        sampling.temp = samplingParams.temp;
        sampling.top_p = samplingParams.topP;
        sampling.top_k = samplingParams.topK;
        session->sampler.reset(common_sampler_init(modelState->model.get(), sampling));
        if (!session->sampler) {
            session->ctx.reset();
            return false;
        }

        session->modelState = modelState;
        session->committedTokens.clear();
        session->kvTokens = 0;
        session->lastSummaryText.clear();
        session->lastSummaryMessageCount = 0;
        session->lastSummaryUpdatedAtUnixMs = 0;
        return true;
    }

    void resetSessionState(std::shared_ptr<ChatSession> const &session) const {
        if (session->ctx) {
            llama_memory_clear(llama_get_memory(session->ctx.get()), true);
        }
        if (session->sampler) {
            common_sampler_reset(session->sampler.get());
        }
    }

    bool decodeTokens(
        llama_context *ctx,
        std::vector<llama_token> const &tokens,
        std::size_t startIndex,
        std::function<bool()> const &isCancelled
    ) const {
        int32_t const tokenCount = static_cast<int32_t>(tokens.size());
        int32_t const batchSize = llama_n_batch(ctx);
        for (int32_t i = static_cast<int32_t>(startIndex); i < tokenCount; i += batchSize) {
            if (isCancelled()) {
                return false;
            }
            int32_t const count = std::min(batchSize, tokenCount - i);
            auto *batchTokens = const_cast<llama_token *>(tokens.data() + i);
            llama_batch const batch = llama_batch_get_one(batchTokens, count);
            if (llama_decode(ctx, batch) != 0) {
                return false;
            }
        }
        return true;
    }

    static void applySummaryToEvent(LlmUiEvent &event, ChatGenerationRequest const &request) {
        event.summaryText = request.summaryText;
        event.summaryMessageCount = request.summaryMessageCount;
        event.summaryUpdatedAtUnixMs = request.summaryUpdatedAtUnixMs;
    }

    void postTerminalEvent(
        std::function<void(LlmUiEvent)> const &postOnMain,
        LlmUiEvent::Kind kind,
        std::string text,
        GenerationStats const &stats,
        ChatGenerationRequest const &request
    ) const {
        LlmUiEvent event {
            .kind = kind,
            .text = std::move(text),
            .generationStats = stats,
        };
        applySummaryToEvent(event, request);
        postOnMain(std::move(event));
    }

    bool compactRequestToFit(
        std::shared_ptr<ChatSession> const &session,
        std::shared_ptr<const LoadedModelState> const &modelState,
        SamplingParams const &samplingParams,
        ChatGenerationRequest &request,
        RenderedPrompt &renderedPrompt,
        std::string &errorText
    ) const {
        if (!session->ctx) {
            errorText = "Failed to create llama context";
            return false;
        }

        std::int32_t const ctxSize = static_cast<std::int32_t>(llama_n_ctx(session->ctx.get()));
        std::int32_t const tokenBudget = std::max<std::int32_t>(1, ctxSize - samplingParams.maxTokens - 64);

        while (true) {
            renderedPrompt = renderPrompt(*modelState, request, true);
            if (renderedPrompt.promptTokens.empty()) {
                errorText = "Empty prompt after tokenization";
                return false;
            }
            if (static_cast<std::int32_t>(renderedPrompt.promptTokens.size()) <= tokenBudget) {
                return true;
            }

            std::size_t const maxEnd = maxSummarizableMessageCount(request);
            if (maxEnd <= request.summaryMessageCount) {
                errorText = "Conversation is too long to fit in context, even after compaction.";
                return false;
            }

            std::size_t const targetEnd = pickSummaryTargetEnd(modelState, request, maxEnd, tokenBudget);
            auto summary = generateSummary(
                modelState,
                request.messages,
                request.summaryMessageCount,
                std::min(targetEnd, maxEnd),
                request.summaryText,
                [&session] { return session->cancelled.load(std::memory_order_relaxed); }
            );
            if (!summary.has_value()) {
                errorText = session->cancelled.load(std::memory_order_relaxed) ?
                                "Generation cancelled" :
                                "Failed to summarize older conversation turns.";
                return false;
            }

            request.summaryText = *summary;
            request.summaryMessageCount = std::min(targetEnd, maxEnd);
            request.summaryUpdatedAtUnixMs = currentUnixMillis();
        }
    }

    void commitSessionState(
        std::shared_ptr<ChatSession> const &session,
        ChatGenerationRequest const &request,
        std::shared_ptr<const LoadedModelState> const &modelState,
        std::string const &assistantText
    ) const {
        ChatGenerationRequest committedRequest = request;
        committedRequest.messages.push_back(ChatMessage {
            .role = ChatMessage::Role::Assistant,
            .text = assistantText,
        });
        auto committedPrompt = renderPrompt(*modelState, committedRequest, false);
        session->committedTokens = std::move(committedPrompt.promptTokens);
        session->kvTokens = session->committedTokens.size();
        session->lastSummaryText = request.summaryText;
        session->lastSummaryMessageCount = request.summaryMessageCount;
        session->lastSummaryUpdatedAtUnixMs = request.summaryUpdatedAtUnixMs;
    }

    void runGeneration(
        std::shared_ptr<ChatSession> const &session,
        ChatGenerationRequest request,
        std::shared_ptr<const LoadedModelState> modelState,
        std::function<void(LlmUiEvent)> userPost
    ) {
        auto postOnMain = [&userPost, chatId = request.chatId, generationId = request.generationId](LlmUiEvent ev) mutable {
            ev.chatId = chatId;
            ev.generationId = generationId;
            detail::postEventToMain(userPost, std::move(ev));
        };
        detail::UiChunkBatcher batcher(postOnMain);
        GenerationStats stats;
        stats.startedAtUnixMs = currentUnixMillis();

        SamplingParams params = currentSamplingParams();
        stats.temp = params.temp;
        stats.topP = params.topP;
        stats.topK = params.topK;
        stats.maxTokens = params.maxTokens;

        if (request.summaryMessageCount > request.messages.size()) {
            request.summaryMessageCount = request.messages.size();
        }

        if (!ensureSessionContext(session, modelState, params)) {
            stats.finishedAtUnixMs = currentUnixMillis();
            stats.status = "error";
            stats.errorText = "Failed to create llama context";
            postTerminalEvent(postOnMain, LlmUiEvent::Kind::Error, stats.errorText, stats, request);
            return;
        }

        RenderedPrompt renderedPrompt;
        std::string compactionError;
        if (!compactRequestToFit(session, modelState, params, request, renderedPrompt, compactionError)) {
            stats.finishedAtUnixMs = currentUnixMillis();
            stats.status = session->cancelled.load(std::memory_order_relaxed) ? "cancelled" : "error";
            stats.errorText = session->cancelled.load(std::memory_order_relaxed) ? std::string() : compactionError;
            stats.promptTokens = static_cast<std::int64_t>(renderedPrompt.promptTokens.size());
            stats.tokensPerSecond = computeTokensPerSecond(stats);
            postTerminalEvent(
                postOnMain,
                session->cancelled.load(std::memory_order_relaxed) ? LlmUiEvent::Kind::Done : LlmUiEvent::Kind::Error,
                compactionError,
                stats,
                request
            );
            session->lastSummaryText = request.summaryText;
            session->lastSummaryMessageCount = request.summaryMessageCount;
            session->lastSummaryUpdatedAtUnixMs = request.summaryUpdatedAtUnixMs;
            session->committedTokens.clear();
            session->kvTokens = 0;
            return;
        }

        stats.promptTokens = static_cast<std::int64_t>(renderedPrompt.promptTokens.size());
        std::fprintf(stderr, "[LlamaEngine] prompt: %lld tokens, max predict: %d\n",
                     static_cast<long long>(stats.promptTokens),
                     params.maxTokens);

        bool const summaryChanged = request.summaryText != session->lastSummaryText ||
                                    request.summaryMessageCount != session->lastSummaryMessageCount ||
                                    request.summaryUpdatedAtUnixMs != session->lastSummaryUpdatedAtUnixMs;
        bool reusePrefix = !summaryChanged && session->modelState == modelState && session->ctx && session->sampler;
        std::size_t commonPrefix = 0;
        if (reusePrefix) {
            commonPrefix = longestCommonPrefix(session->committedTokens, renderedPrompt.promptTokens);
            commonPrefix = std::min(commonPrefix, session->kvTokens);
        }

        if (!reusePrefix) {
            resetSessionState(session);
            session->committedTokens.clear();
            session->kvTokens = 0;
            commonPrefix = 0;
        } else if (commonPrefix < session->kvTokens) {
            if (!llama_memory_seq_rm(llama_get_memory(session->ctx.get()), 0, static_cast<llama_pos>(commonPrefix), -1)) {
                resetSessionState(session);
                session->committedTokens.clear();
                session->kvTokens = 0;
                commonPrefix = 0;
            }
        }

        common_sampler_reset(session->sampler.get());
        if (!decodeTokens(
                session->ctx.get(),
                renderedPrompt.promptTokens,
                commonPrefix,
                [&session] { return session->cancelled.load(std::memory_order_relaxed); })) {
            batcher.flush();
            stats.finishedAtUnixMs = currentUnixMillis();
            stats.status = session->cancelled.load(std::memory_order_relaxed) ? "cancelled" : "error";
            stats.errorText = session->cancelled.load(std::memory_order_relaxed) ? std::string() : decodeError("prompt evaluation");
            stats.tokensPerSecond = computeTokensPerSecond(stats);
            postTerminalEvent(
                postOnMain,
                session->cancelled.load(std::memory_order_relaxed) ? LlmUiEvent::Kind::Done : LlmUiEvent::Kind::Error,
                stats.errorText,
                stats,
                request
            );
            session->lastSummaryText = request.summaryText;
            session->lastSummaryMessageCount = request.summaryMessageCount;
            session->lastSummaryUpdatedAtUnixMs = request.summaryUpdatedAtUnixMs;
            session->committedTokens.clear();
            session->kvTokens = 0;
            return;
        }

        common_chat_parser_params parserParams(renderedPrompt.chatParams);
        parserParams.reasoning_format = COMMON_REASONING_FORMAT_DEEPSEEK;
        if (!renderedPrompt.chatParams.parser.empty()) {
            parserParams.parser.load(renderedPrompt.chatParams.parser);
        }
        detail::StreamingParseAccumulator accumulator(parserParams, batcher);

        for (int i = 0; i < params.maxTokens; ++i) {
            if (session->cancelled.load(std::memory_order_relaxed)) {
                accumulator.flush();
                batcher.flush();
                stats.finishedAtUnixMs = currentUnixMillis();
                stats.status = "cancelled";
                stats.tokensPerSecond = computeTokensPerSecond(stats);
                postTerminalEvent(postOnMain, LlmUiEvent::Kind::Done, "", stats, request);
                session->lastSummaryText = request.summaryText;
                session->lastSummaryMessageCount = request.summaryMessageCount;
                session->lastSummaryUpdatedAtUnixMs = request.summaryUpdatedAtUnixMs;
                session->committedTokens.clear();
                session->kvTokens = 0;
                return;
            }

            llama_token token = common_sampler_sample(session->sampler.get(), session->ctx.get(), -1);
            if (llama_vocab_is_eog(modelState->vocab, token)) {
                stats.status = "completed";
                break;
            }

            common_sampler_accept(session->sampler.get(), token, true);
            if (stats.firstTokenAtUnixMs == 0) {
                stats.firstTokenAtUnixMs = currentUnixMillis();
            }
            ++stats.completionTokens;
            accumulator.append(tokenToPiece(modelState->vocab, token));

            llama_batch const batch = llama_batch_get_one(&token, 1);
            if (llama_decode(session->ctx.get(), batch) != 0) {
                accumulator.flush();
                batcher.flush();
                stats.finishedAtUnixMs = currentUnixMillis();
                stats.status = "error";
                stats.errorText = decodeError("generation");
                stats.tokensPerSecond = computeTokensPerSecond(stats);
                postTerminalEvent(postOnMain, LlmUiEvent::Kind::Error, stats.errorText, stats, request);
                session->lastSummaryText = request.summaryText;
                session->lastSummaryMessageCount = request.summaryMessageCount;
                session->lastSummaryUpdatedAtUnixMs = request.summaryUpdatedAtUnixMs;
                session->committedTokens.clear();
                session->kvTokens = 0;
                return;
            }
        }

        if (stats.status.empty()) {
            stats.status = "max_tokens";
        }

        accumulator.flush();
        batcher.flush();
        stats.finishedAtUnixMs = currentUnixMillis();
        stats.tokensPerSecond = computeTokensPerSecond(stats);
        commitSessionState(session, request, modelState, accumulator.parsedMessage().content);
        postTerminalEvent(postOnMain, LlmUiEvent::Kind::Done, "", stats, request);
    }

    void runFakeGeneration(
        std::shared_ptr<ChatSession> const &session,
        ChatGenerationRequest const &request,
        std::function<void(LlmUiEvent)> userPost
    ) {
        auto postOnMain = [&userPost, chatId = request.chatId, generationId = request.generationId](LlmUiEvent ev) mutable {
            ev.chatId = chatId;
            ev.generationId = generationId;
            detail::postEventToMain(userPost, std::move(ev));
        };
        detail::UiChunkBatcher batcher(postOnMain);
        GenerationStats stats;
        stats.startedAtUnixMs = currentUnixMillis();
        stats.status = "completed";
        stats.temp = 0.8f;
        stats.topP = 0.95f;
        stats.topK = 40;
        stats.maxTokens = 4096;

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
            if (session->cancelled.load(std::memory_order_relaxed)) {
                batcher.flush();
                stats.finishedAtUnixMs = currentUnixMillis();
                stats.status = "cancelled";
                stats.tokensPerSecond = computeTokensPerSecond(stats);
                postTerminalEvent(postOnMain, LlmUiEvent::Kind::Done, "", stats, request);
                return;
            }

            if (stats.firstTokenAtUnixMs == 0) {
                stats.firstTokenAtUnixMs = currentUnixMillis();
            }
            ++stats.completionTokens;
            batcher.push(part, text);
            std::this_thread::sleep_for(delay);
        }

        batcher.flush();
        stats.finishedAtUnixMs = currentUnixMillis();
        stats.tokensPerSecond = computeTokensPerSecond(stats);
        postTerminalEvent(postOnMain, LlmUiEvent::Kind::Done, "", stats, request);
    }

    mutable std::mutex mutex_;
    common_params params_;
    std::shared_ptr<const LoadedModelState> modelState_;
    std::string modelPath_;
    SamplingParams samplingParams_;
    std::unordered_map<std::string, std::shared_ptr<ChatSession>> sessions_;
};

} // namespace lambda_studio_backend
