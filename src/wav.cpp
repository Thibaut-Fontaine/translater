#include "xtrans/wav.hpp"

#include <cstdio>
#include <cstring>

namespace xtrans {
namespace {

void put_u32(unsigned char* p, std::uint32_t v) noexcept {
  p[0] = static_cast<unsigned char>(v & 0xFFU);
  p[1] = static_cast<unsigned char>((v >> 8) & 0xFFU);
  p[2] = static_cast<unsigned char>((v >> 16) & 0xFFU);
  p[3] = static_cast<unsigned char>((v >> 24) & 0xFFU);
}

void put_u16(unsigned char* p, std::uint16_t v) noexcept {
  p[0] = static_cast<unsigned char>(v & 0xFFU);
  p[1] = static_cast<unsigned char>((v >> 8) & 0xFFU);
}

}  // namespace

std::size_t write_wav(const char* path, std::span<const std::int16_t> pcm,
                      std::uint32_t sample_rate) noexcept {
  std::FILE* file = std::fopen(path, "wb");
  if (file == nullptr) return 0;

  const std::uint32_t data_bytes = static_cast<std::uint32_t>(pcm.size() * 2);
  unsigned char header[44];
  std::memcpy(header, "RIFF", 4);
  put_u32(header + 4, 36 + data_bytes);
  std::memcpy(header + 8, "WAVE", 4);
  std::memcpy(header + 12, "fmt ", 4);
  put_u32(header + 16, 16);                              // fmt chunk size
  put_u16(header + 20, 1);                               // PCM
  put_u16(header + 22, 1);                               // mono
  put_u32(header + 24, sample_rate);
  put_u32(header + 28, sample_rate * 2);                 // byte rate
  put_u16(header + 32, 2);                               // block align
  put_u16(header + 34, 16);                              // bits per sample
  std::memcpy(header + 36, "data", 4);
  put_u32(header + 40, data_bytes);

  std::size_t written = std::fwrite(header, 1, sizeof header, file);
  if (!pcm.empty()) {
    // fwrite returns elements, not bytes.
    written += std::fwrite(pcm.data(), 2, pcm.size(), file) * 2;
  }
  std::fclose(file);
  return written;
}

}  // namespace xtrans
