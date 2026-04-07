#pragma once

/// \file LlamaEngine.hpp
///
/// In-process llama.cpp inference engine for LLM Studio.
/// Uses the same common_params / common_context_params_to_llama /
/// common_sampler_init path as llama-cli so that context creation,
/// KV-cache layout, and sampler configuration are identical.

#include "common.h"
#include "chat.h"
#include "sampling.h"

#include <algorithm>
#include <atomic>
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

#if defined(__APPLE__)
#include <dispatch/dispatch.h>
#endif

#include "Types.hpp"

namespace llm_studio {

// ── UI event ────────────────────────────────────────────────────────────────

struct LlmUiEvent {
  enum class Kind { Chunk, Done, Error };
  enum class Part { Content, Thinking };
  Kind kind = Kind::Done;
  Part part = Part::Content;
  std::string chatId;
  std::string text;
};

// ── Main-thread dispatch ────────────────────────────────────────────────────

namespace detail {

struct MainPostThunk {
  std::function<void(LlmUiEvent)> post;
  LlmUiEvent ev;
};

#if defined(__APPLE__)
inline void invokePostOnMain(void* ctx) {
  auto* p = static_cast<MainPostThunk*>(ctx);
  p->post(std::move(p->ev));
  delete p;
}
#endif

inline void postEventToMain(std::function<void(LlmUiEvent)> const& post, LlmUiEvent ev) {
#if defined(__APPLE__)
  auto* ctx = new MainPostThunk{post, std::move(ev)};
  dispatch_async_f(dispatch_get_main_queue(), ctx, invokePostOnMain);
#else
  post(std::move(ev));
#endif
}

} // namespace detail

// ── Token → string helper (for debug logging) ───────────────────────────────

inline std::string tokenToPiece(const llama_vocab* vocab, llama_token token) {
  char buf[256];
  int n = llama_token_to_piece(vocab, token, buf, sizeof(buf), 0, /*special=*/true);
  if (n < 0) {
    std::string s(static_cast<std::size_t>(-n), '\0');
    llama_token_to_piece(vocab, token, s.data(), static_cast<int32_t>(s.size()), 0, /*special=*/true);
    return s;
  }
  return std::string(buf, static_cast<std::size_t>(n));
}

// ── Engine ──────────────────────────────────────────────────────────────────

/// Holds the loaded model and provides chat completion on a worker thread.
/// One instance per application lifetime. Thread-safe for concurrent chat
/// requests (only one generation runs at a time; new requests cancel the
/// previous one).
class LlamaEngine {
public:
  LlamaEngine() = default;
  ~LlamaEngine() {
    cancelGeneration();
    joinWorker();
    unload();
  }

  LlamaEngine(LlamaEngine const&) = delete;
  LlamaEngine& operator=(LlamaEngine const&) = delete;

  /// Load a GGUF model file. Blocks until the model is ready.
  /// Returns true on success.
  bool load(std::string const& modelPath, int nGpuLayers = -1, uint32_t nCtx = 0) {
    std::lock_guard<std::mutex> lock(mutex_);
    unloadLocked();

    ggml_backend_load_all();
    llama_backend_init();

    // Build the same common_params the CLI would use.
    params_ = common_params{};
    params_.model.path  = modelPath;
    params_.n_gpu_layers = (nGpuLayers < 0) ? 999 : nGpuLayers;
    params_.n_ctx        = nCtx; // 0 → use model's n_ctx_train

    auto mparams = common_model_params_to_llama(params_);

    model_.reset(llama_model_load_from_file(modelPath.c_str(), mparams));
    if (!model_) {
      std::fprintf(stderr, "[LlamaEngine] failed to load model: %s\n", modelPath.c_str());
      return false;
    }

    vocab_     = llama_model_get_vocab(model_.get());
    modelPath_ = modelPath;

    templates_ = common_chat_templates_init(model_.get(), /*chat_template_override=*/"");

    std::fprintf(stderr, "[LlamaEngine] loaded: %s (n_gpu_layers=%d, n_ctx=%u)\n",
                 modelPath.c_str(), params_.n_gpu_layers, nCtx);
    std::fprintf(stderr, "[LlamaEngine] chat template source: %.120s\n",
                 common_chat_templates_source(templates_.get()).c_str());
    return true;
  }

  bool isLoaded() const { return model_ != nullptr; }

  std::string const& modelPath() const { return modelPath_; }

  SamplingParams samplingParams() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return samplingParams_;
  }

  void setSamplingParams(SamplingParams const& sp) {
    std::lock_guard<std::mutex> lock(mutex_);
    samplingParams_ = sp;
  }

  void unload() {
    std::lock_guard<std::mutex> lock(mutex_);
    unloadLocked();
  }

  void cancelGeneration() {
    cancelled_.store(true, std::memory_order_relaxed);
  }

  /// Start a streaming chat completion on a worker thread.
  /// `post` is called on the main thread (via GCD) for each token and on
  /// completion.
  void startChat(std::vector<ChatMessage> messages, std::string chatId,
                 std::function<void(LlmUiEvent)> post) {
    if (!model_) {
      detail::postEventToMain(post, LlmUiEvent{
          .kind = LlmUiEvent::Kind::Error,
          .chatId = chatId,
          .text = "No model loaded",
      });
      return;
    }

    cancelGeneration();
    joinWorker();

    cancelled_.store(false, std::memory_order_relaxed);

    worker_ = std::thread([this,
                            msgs = std::move(messages),
                            chatId = std::move(chatId),
                            userPost = std::move(post)]() mutable {
      runGeneration(std::move(msgs), std::move(chatId), std::move(userPost));
    });
  }

private:
  void joinWorker() {
    if (worker_.joinable()) worker_.join();
  }

  void unloadLocked() {
    cancelGeneration();
    joinWorker();
    templates_.reset();
    vocab_ = nullptr;
    model_.reset();
    modelPath_.clear();
  }

  void runGeneration(std::vector<ChatMessage> messages, std::string chatId,
                     std::function<void(LlmUiEvent)> userPost) {
    auto postOnMain = [&userPost, &chatId](LlmUiEvent ev) {
      ev.chatId = chatId;
      detail::postEventToMain(userPost, std::move(ev));
    };

    // ── 1. Build chat messages (skip Reasoning entries and trailing
    //        empty Assistant placeholders) ──────────────────────────────
    std::vector<common_chat_msg> chatMsgs;
    chatMsgs.reserve(messages.size());
    for (auto const& m : messages) {
      if (m.role == ChatMessage::Role::Reasoning) continue;
      if (m.role == ChatMessage::Role::Assistant && m.text.empty()) continue;
      common_chat_msg cm;
      cm.role    = (m.role == ChatMessage::Role::User) ? "user" : "assistant";
      cm.content = m.text;
      chatMsgs.push_back(std::move(cm));
    }
    while (!chatMsgs.empty() && chatMsgs.back().role == "assistant" && chatMsgs.back().content.empty())
      chatMsgs.pop_back();

    // ── 2. Apply Jinja2 chat template ───────────────────────────────────
    common_chat_templates_inputs inputs;
    inputs.messages              = chatMsgs;
    inputs.add_generation_prompt = true;
    inputs.use_jinja             = true;
    inputs.reasoning_format      = COMMON_REASONING_FORMAT_DEEPSEEK;
    inputs.enable_thinking       = common_chat_templates_support_enable_thinking(templates_.get());

    common_chat_params cp = common_chat_templates_apply(templates_.get(), inputs);

    std::fprintf(stderr, "[LlamaEngine] template format=%s supports_thinking=%d\n",
                 common_chat_format_name(cp.format), (int)cp.supports_thinking);
    std::fprintf(stderr, "[LlamaEngine] prompt (first 200 chars): %.200s\n", cp.prompt.c_str());

    // ── 3. Tokenize formatted prompt ────────────────────────────────────
    auto promptTokens = common_tokenize(vocab_, cp.prompt, /*add_special=*/true, /*parse_special=*/true);

    if (promptTokens.empty()) {
      postOnMain(LlmUiEvent{.kind = LlmUiEvent::Kind::Error, .text = "Empty prompt after tokenization"});
      return;
    }

    SamplingParams sp;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      sp = samplingParams_;
    }

    int const nPrompt  = static_cast<int>(promptTokens.size());
    int const nPredict = sp.maxTokens;

    std::fprintf(stderr, "[LlamaEngine] prompt: %d tokens, max predict: %d\n", nPrompt, nPredict);

    // ── 4. Create context — same path as llama-cli ──────────────────────
    //  n_ctx = 0 tells llama.cpp to use the model's n_ctx_train.
    //  Do NOT override to a small value: the dk512 flash-attention vec
    //  kernel triggers a GPU hang on Apple M3 Pro at small context sizes.
    auto cparams = common_context_params_to_llama(params_);

    llama_context_ptr ctx(llama_init_from_model(model_.get(), cparams));
    if (!ctx) {
      postOnMain(LlmUiEvent{.kind = LlmUiEvent::Kind::Error, .text = "Failed to create llama context"});
      return;
    }

    // ── 5. Sampler — apply user-configured sampling parameters ──────────
    // common_sampler_init mutates its argument. Use a fresh struct each run so we
    // never persist llama.cpp mutations inside params_.sampling.
    common_params_sampling samp{};
    samp.temp  = sp.temp;
    samp.top_p = sp.topP;
    samp.top_k = sp.topK;
    common_sampler_ptr smpl(common_sampler_init(model_.get(), samp));

    // ── 6. Prompt evaluation (prefill) ───────────────────────────────────
    {
      const int32_t nBatch = llama_n_batch(ctx.get());
      for (int32_t i = 0; i < nPrompt; i += nBatch) {
        const int32_t nTokens = std::min(nBatch, nPrompt - i);
        llama_batch batch = llama_batch_get_one(promptTokens.data() + i, nTokens);
        if (llama_decode(ctx.get(), batch) != 0) {
          postOnMain(LlmUiEvent{.kind = LlmUiEvent::Kind::Error,
              .text = decodeError("prompt evaluation")});
          return;
        }
        if (cancelled_.load(std::memory_order_relaxed)) {
          postOnMain(LlmUiEvent{.kind = LlmUiEvent::Kind::Done});
          return;
        }
      }
    }

    // ── 7. Streaming generation with PEG-based reasoning/content parsing ──
    common_chat_parser_params pp(cp);
    pp.reasoning_format = COMMON_REASONING_FORMAT_DEEPSEEK;
    if (!cp.parser.empty()) {
      pp.parser.load(cp.parser);
    }

    std::string     rawOutput;
    common_chat_msg prevMsg;
    prevMsg.role = "assistant";

    for (int i = 0; i < nPredict; ++i) {
      if (cancelled_.load(std::memory_order_relaxed)) {
        postOnMain(LlmUiEvent{.kind = LlmUiEvent::Kind::Done});
        return;
      }

      llama_token newToken = common_sampler_sample(smpl.get(), ctx.get(), -1);

      if (llama_vocab_is_eog(vocab_, newToken)) {
        std::fprintf(stderr, "[LlamaEngine] EOG token=%d at position %d\n", (int)newToken, i);
        break;
      }

      common_sampler_accept(smpl.get(), newToken, /*accept_grammar=*/true);

      std::string piece = tokenToPiece(vocab_, newToken);
      rawOutput += piece;

      auto msg   = common_chat_parse(rawOutput, /*is_partial=*/true, pp);
      auto diffs = common_chat_msg_diff::compute_diffs(prevMsg, msg);
      for (auto& d : diffs) {
        if (!d.reasoning_content_delta.empty()) {
          postOnMain(LlmUiEvent{
              .kind = LlmUiEvent::Kind::Chunk,
              .part = LlmUiEvent::Part::Thinking,
              .text = std::move(d.reasoning_content_delta),
          });
        }
        if (!d.content_delta.empty()) {
          postOnMain(LlmUiEvent{
              .kind = LlmUiEvent::Kind::Chunk,
              .part = LlmUiEvent::Part::Content,
              .text = std::move(d.content_delta),
          });
        }
      }
      prevMsg = std::move(msg);

      llama_batch genBatch = llama_batch_get_one(&newToken, 1);

      if (llama_decode(ctx.get(), genBatch) != 0) {
        postOnMain(LlmUiEvent{.kind = LlmUiEvent::Kind::Error,
            .text = decodeError("generation")});
        return;
      }
    }

    std::fprintf(stderr, "[LlamaEngine] generation done. raw output (first 500 chars):\n%.500s\n[END]\n",
                 rawOutput.c_str());

    postOnMain(LlmUiEvent{.kind = LlmUiEvent::Kind::Done});
  }

  std::string decodeError(char const* phase) const {
    std::string msg = std::string("Decode failed during ") + phase + ".";
    if (params_.n_gpu_layers != 0) {
      msg += " If you see a GPU Hang in the logs, re-run with"
             " LLAMA_N_GPU_LAYERS=0 to use CPU-only inference.";
    }
    return msg;
  }

  mutable std::mutex         mutex_;
  common_params              params_;
  llama_model_ptr            model_;
  common_chat_templates_ptr  templates_;
  const llama_vocab*         vocab_ = nullptr;
  std::string                modelPath_;
  SamplingParams             samplingParams_;
  std::atomic<bool>          cancelled_{false};
  std::thread                worker_;
};

// ── Defaults (env vars) ─────────────────────────────────────────────────────

inline std::string defaultModelPath() {
  if (char const* p = std::getenv("LLAMA_MODEL_PATH"))
    return std::string(p);
  return {};
}

inline std::string defaultModelName() {
  if (char const* n = std::getenv("LLAMA_MODEL_NAME"))
    return std::string(n);
  return "local";
}

/// Number of model layers to offload to GPU.
/// -1 (default) = all layers on GPU.
///  0            = CPU-only inference.
/// Set via the LLAMA_N_GPU_LAYERS environment variable.
inline int defaultNGpuLayers() {
  if (char const* s = std::getenv("LLAMA_N_GPU_LAYERS"))
    return std::atoi(s);
  return -1;
}

} // namespace llm_studio
