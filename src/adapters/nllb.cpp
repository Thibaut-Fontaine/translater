#include "xtrans/adapters/nllb.hpp"

#include <onnxruntime/onnxruntime_cxx_api.h>
#include <sentencepiece_processor.h>

#include <algorithm>
#include <cstring>
#include <exception>
#include <optional>
#include <string>
#include <vector>

#include "xtrans/nllb_ids.hpp"

namespace xtrans {
namespace {

constexpr int kMaxNewTokens = 128;
constexpr std::size_t kNllbMaxInput = 1024;  // trained position limit
constexpr const char* kPastPrefix = "past_key_values.";
constexpr const char* kPresentPrefix = "present.";

Ort::SessionOptions make_options(int threads) {
  Ort::SessionOptions options;
  options.SetIntraOpNumThreads(threads);
  options.SetGraphOptimizationLevel(ORT_ENABLE_ALL);
  return options;
}

std::int64_t argmax(const float* row, std::int64_t size) noexcept {
  std::int64_t best = 0;
  for (std::int64_t i = 1; i < size; ++i) {
    if (row[i] > row[best]) best = i;
  }
  return best;
}

bool starts_with(const char* text, const char* prefix) noexcept {
  return std::strncmp(text, prefix, std::strlen(prefix)) == 0;
}

// Maps "present.<suffix>" to the cache slot owned by "past_key_values.<suffix>".
int slot_of(const std::string& suffix, const std::vector<std::string>& cache_inputs) {
  const std::string probe = std::string(kPastPrefix) + suffix;
  for (std::size_t i = 0; i < cache_inputs.size(); ++i) {
    if (cache_inputs[i] == probe) return static_cast<int>(i);
  }
  return -1;
}

// One decoder variant: which cache inputs it takes and which present outputs it
// returns, in request order.
struct CacheIO {
  std::vector<std::string> request;  // output names, logits first
  std::vector<int> slots;            // cache slot for each request[1..]
};

bool build_cache_io(Ort::Session& session, std::vector<std::string>& cache_inputs,
                    bool emit_inputs, CacheIO& io, std::string& error) {
  const Ort::AllocatorWithDefaultOptions allocator;

  if (emit_inputs) {
    for (std::size_t i = 0; i < session.GetInputCount(); ++i) {
      const auto name = session.GetInputNameAllocated(i, allocator);
      if (starts_with(name.get(), kPastPrefix)) cache_inputs.emplace_back(name.get());
    }
    if (cache_inputs.empty()) {
      error = "decoder_with_past has no cache inputs (wrong export?)";
      return false;
    }
  }

  io.request.emplace_back("logits");
  for (std::size_t i = 0; i < session.GetOutputCount(); ++i) {
    const auto name = session.GetOutputNameAllocated(i, allocator);
    const std::string text = name.get();
    if (!starts_with(name.get(), kPresentPrefix)) continue;

    const std::string suffix = text.substr(std::strlen(kPresentPrefix));
    const int slot = slot_of(suffix, cache_inputs);
    if (slot < 0) {
      error = "cache output without input slot: " + text;
      return false;
    }
    io.request.push_back(text);
    io.slots.push_back(slot);
  }

  if (io.slots.empty()) {
    error = "decoder exposes no present outputs (wrong export?)";
    return false;
  }
  return true;
}

}  // namespace

struct NllbMt::Impl {
  explicit Impl(int threads)
      : env(ORT_LOGGING_LEVEL_WARNING, "xtrans"),
        options(make_options(threads)),
        memory(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault)) {}

  Ort::Env env;
  Ort::SessionOptions options;
  std::unique_ptr<Ort::Session> encoder;
  std::unique_ptr<Ort::Session> decoder;            // first step, no cache
  std::unique_ptr<Ort::Session> decoder_with_past;  // later steps, KV cache
  Ort::MemoryInfo memory;
  sentencepiece::SentencePieceProcessor tokenizer;

  std::vector<std::string> cache_inputs;  // past_key_values.*, cache order
  CacheIO first_io;
  CacheIO cached_io;

  std::vector<std::optional<Ort::Value>> cache;  // one slot per cache input
  std::vector<bool> updated;

  std::string input_text;
  std::vector<int> pieces;
  std::vector<std::int64_t> source_ids;
  std::vector<std::int64_t> decoder_ids;
  std::vector<std::int64_t> encoder_mask;
  std::vector<std::int64_t> generated;
  std::vector<std::int32_t> token_ids;
  std::vector<int> sp_ids;
  std::string output_text;

  std::size_t last_tokens = 0;
};

NllbMt::NllbMt() : impl_(std::make_unique<Impl>(NllbConfig{}.threads)) {}

NllbMt::~NllbMt() = default;

std::unique_ptr<NllbMt> NllbMt::open(const NllbConfig& config, std::string& error) noexcept {
  std::unique_ptr<NllbMt> self(new (std::nothrow) NllbMt());
  if (!self) {
    error = "out of memory";
    return nullptr;
  }

  try {
    auto& impl = *self->impl_;
    impl.options = make_options(config.threads);
    impl.encoder = std::make_unique<Ort::Session>(impl.env, config.encoder, impl.options);
    impl.decoder = std::make_unique<Ort::Session>(impl.env, config.decoder, impl.options);
    impl.decoder_with_past =
        std::make_unique<Ort::Session>(impl.env, config.decoder_with_past, impl.options);

    const auto tokenizer_status = impl.tokenizer.Load(config.tokenizer);
    if (!tokenizer_status.ok()) {
      error = tokenizer_status.ToString();
      return nullptr;
    }

    if (!build_cache_io(*impl.decoder_with_past, impl.cache_inputs, true, impl.cached_io, error)) {
      return nullptr;
    }
    if (!build_cache_io(*impl.decoder, impl.cache_inputs, false, impl.first_io, error)) {
      return nullptr;
    }
    if (impl.first_io.slots.size() != impl.cache_inputs.size()) {
      error = "first-step decoder does not fill the whole cache";
      return nullptr;
    }

    impl.cache.resize(impl.cache_inputs.size());
    impl.updated.assign(impl.cache_inputs.size(), false);
    impl.pieces.reserve(256);
    impl.generated.reserve(kMaxNewTokens);
    impl.source_ids.reserve(kNllbMaxInput);
    impl.encoder_mask.reserve(kNllbMaxInput);
  } catch (const std::exception& e) {
    error = e.what();
    return nullptr;
  } catch (...) {
    error = "unknown ONNX Runtime failure";
    return nullptr;
  }

  return self;
}

MtStatus NllbMt::translate(std::string_view text, Lang from, Lang to, std::span<char> out,
                           std::size_t& written) noexcept {
  written = 0;
  // A voice is not required: the phone app shows text, so any pair with NLLB
  // tokens on both sides is translatable.
  if (!can_translate(from, to)) return MtStatus::unsupported;

  const std::int32_t source_lang = lang_info(from).nllb_id;
  const std::int32_t target_lang = lang_info(to).nllb_id;
  if (source_lang == 0 || target_lang == 0) return MtStatus::unsupported;
  if (text.empty()) return MtStatus::input_empty;
  if (out.empty()) return MtStatus::failed;

  try {
    auto& impl = *impl_;

    impl.input_text.assign(text);
    impl.pieces.clear();
    if (!impl.tokenizer.Encode(impl.input_text, &impl.pieces).ok()) return MtStatus::failed;

    impl.source_ids.resize(impl.pieces.size() + 2);
    const std::size_t source_len =
        nllb_build_source_ids(impl.pieces, source_lang, impl.source_ids);
    if (source_len == 0 || source_len > kNllbMaxInput) return MtStatus::failed;
    impl.source_ids.resize(source_len);

    const std::int64_t source_shape[2] = {1, static_cast<std::int64_t>(source_len)};
    impl.encoder_mask.assign(source_len, 1);
    Ort::Value source_tensor = Ort::Value::CreateTensor<std::int64_t>(
        impl.memory, impl.source_ids.data(), impl.source_ids.size(), source_shape, 2);
    Ort::Value mask_tensor = Ort::Value::CreateTensor<std::int64_t>(
        impl.memory, impl.encoder_mask.data(), impl.encoder_mask.size(), source_shape, 2);
    const char* encoder_inputs[] = {"input_ids", "attention_mask"};
    const char* encoder_outputs[] = {"last_hidden_state"};
    Ort::Value encoder_feed[] = {std::move(source_tensor), std::move(mask_tensor)};

    auto encoded = impl.encoder->Run(Ort::RunOptions{nullptr}, encoder_inputs, encoder_feed, 2,
                                     encoder_outputs, 1);
    const auto hidden_info = encoded[0].GetTensorTypeAndShapeInfo();
    const std::vector<std::int64_t> hidden_shape = hidden_info.GetShape();
    float* hidden = encoded[0].GetTensorMutableData<float>();
    const std::size_t hidden_count = static_cast<std::size_t>(hidden_info.GetElementCount());

    const std::int64_t decoder_mask_shape[2] = {1, hidden_shape[1]};
    impl.encoder_mask.assign(static_cast<std::size_t>(hidden_shape[1]), 1);

    impl.decoder_ids.assign(1, kNllbEos);
    impl.generated.clear();
    impl.last_tokens = 0;
    for (auto& slot : impl.cache) slot.reset();

    std::vector<const char*> cached_input_names{"encoder_attention_mask", "input_ids"};
    cached_input_names.reserve(2 + impl.cache_inputs.size());
    for (const auto& name : impl.cache_inputs) cached_input_names.push_back(name.c_str());

    std::vector<const char*> first_outputs;
    for (const auto& name : impl.first_io.request) first_outputs.push_back(name.c_str());
    std::vector<const char*> cached_outputs;
    for (const auto& name : impl.cached_io.request) cached_outputs.push_back(name.c_str());

    std::vector<Ort::Value> feed;
    feed.reserve(cached_input_names.size());

    for (int step = 0; step < kMaxNewTokens; ++step) {
      const bool cached = step != 0;
      const std::int64_t ids_shape[2] = {1,
                                         static_cast<std::int64_t>(impl.decoder_ids.size())};
      Ort::Value mask_value = Ort::Value::CreateTensor<std::int64_t>(
          impl.memory, impl.encoder_mask.data(), impl.encoder_mask.size(), decoder_mask_shape, 2);
      Ort::Value ids_value = Ort::Value::CreateTensor<std::int64_t>(
          impl.memory, impl.decoder_ids.data(), impl.decoder_ids.size(), ids_shape, 2);

      std::vector<Ort::Value> decoded;
      if (!cached) {
        Ort::Value hidden_value = Ort::Value::CreateTensor<float>(
            impl.memory, hidden, hidden_count, hidden_shape.data(), hidden_shape.size());
        const char* names[] = {"encoder_attention_mask", "input_ids", "encoder_hidden_states"};
        Ort::Value values[] = {std::move(mask_value), std::move(ids_value),
                               std::move(hidden_value)};
        decoded = impl.decoder->Run(Ort::RunOptions{nullptr}, names, values, 3,
                                    first_outputs.data(), first_outputs.size());
      } else {
        std::fill(impl.updated.begin(), impl.updated.end(), false);
        feed.clear();
        feed.push_back(std::move(mask_value));
        feed.push_back(std::move(ids_value));
        for (auto& slot : impl.cache) {
          feed.push_back(std::move(*slot));
          slot.reset();
        }
        decoded = impl.decoder_with_past->Run(Ort::RunOptions{nullptr}, cached_input_names.data(),
                                              feed.data(), feed.size(), cached_outputs.data(),
                                              cached_outputs.size());
        // The with-past decoder only refreshes the self-attention cache; the
        // cross-attention entries from the first step stay in place.
        for (std::size_t i = 0; i < impl.cached_io.slots.size(); ++i) {
          const auto slot = static_cast<std::size_t>(impl.cached_io.slots[i]);
          impl.cache[slot] = std::move(decoded[i + 1]);
          impl.updated[slot] = true;
        }
        for (std::size_t i = 0; i < impl.cache.size(); ++i) {
          if (!impl.updated[i]) impl.cache[i] = std::move(feed[2 + i]);
        }
      }

      if (!cached) {
        for (std::size_t i = 0; i < impl.first_io.slots.size(); ++i) {
          const auto slot = static_cast<std::size_t>(impl.first_io.slots[i]);
          impl.cache[slot] = std::move(decoded[i + 1]);
        }
      }

      const auto logits_info = decoded[0].GetTensorTypeAndShapeInfo();
      const std::vector<std::int64_t> logits_shape = logits_info.GetShape();
      const std::int64_t vocab = logits_shape[2];
      const std::int64_t last = logits_shape[1] - 1;
      const float* row = decoded[0].GetTensorData<float>() + last * vocab;

      std::int64_t best = argmax(row, vocab);
      if (step == 0) best = target_lang;  // forced BOS: the target language token
      if (best == kNllbEos) break;

      impl.generated.push_back(best);
      impl.decoder_ids.assign(1, best);  // the cache carries the rest
    }

    impl.last_tokens = impl.generated.size();
    impl.token_ids.resize(impl.generated.size());
    const std::size_t piece_count = nllb_filter_output(impl.generated, impl.token_ids);
    impl.sp_ids.assign(impl.token_ids.begin(),
                       impl.token_ids.begin() + static_cast<std::ptrdiff_t>(piece_count));

    impl.output_text.clear();
    if (!impl.tokenizer.Decode(impl.sp_ids, &impl.output_text).ok()) return MtStatus::failed;
    if (impl.output_text.empty()) return MtStatus::failed;

    const std::size_t take = std::min(impl.output_text.size(), out.size());
    std::memcpy(out.data(), impl.output_text.data(), take);
    written = take;
    return MtStatus::ok;
  } catch (const std::exception&) {
    return MtStatus::failed;
  } catch (...) {
    return MtStatus::failed;
  }
}

std::size_t NllbMt::last_tokens() const noexcept { return impl_->last_tokens; }

}  // namespace xtrans
