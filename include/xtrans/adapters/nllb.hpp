#pragma once

#if !defined(XTRANS_WITH_NLLB) || !XTRANS_WITH_NLLB
#error "xtrans/adapters/nllb.hpp requires a build with XTRANS_WITH_NLLB=1"
#endif

#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <string_view>

#include "xtrans/mt.hpp"

namespace xtrans {

struct NllbConfig {
  const char* encoder = nullptr;           // encoder_model_int8.onnx
  const char* decoder = nullptr;           // decoder_model_quantized.onnx (first step)
  const char* decoder_with_past = nullptr; // decoder_with_past_model_quantized.onnx
  const char* tokenizer = nullptr;         // sentencepiece.bpe.model or the BPE blob
  int threads = 4;
  // True on devices with room for all three sessions (~1.3 GB). False opens
  // one session per stage and releases it, which keeps the peak near 500 MB
  // but costs a re-read of the weights on the next turn.
  bool cache_sessions = true;
};

// NLLB-200-distilled-600M as an ONNX encoder/decoder pair.
//
// ONNX Runtime is delay-loaded (see ort_delay_load.cpp): its bundled protobuf
// must not interpose on the system protobuf that libsentencepiece uses. The
// ORT C++ API throws, so this translation unit is the only one compiled with
// exceptions enabled; every failure is caught here and returned as MtStatus.
class NllbMt final : public Mt {
 public:
  static std::unique_ptr<NllbMt> open(const NllbConfig& config, std::string& error) noexcept;

  ~NllbMt() override;

  NllbMt(const NllbMt&) = delete;
  NllbMt& operator=(const NllbMt&) = delete;

  MtStatus translate(std::string_view text, Lang from, Lang to, std::span<char> out,
                     std::size_t& written) noexcept override;

  // Decoder steps used by the last translation (benchmark fuel).
  std::size_t last_tokens() const noexcept;

 private:
  NllbMt();
  bool ensure_encoder(std::string& error) noexcept;
  bool ensure_decoder(std::string& error) noexcept;
  bool ensure_with_past(std::string& error) noexcept;
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace xtrans
