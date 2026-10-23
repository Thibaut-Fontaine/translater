#include "harness.hpp"
#include "xtrans/wav.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace xtrans;

namespace {

struct Header {
  char riff[4];
  std::uint32_t size;
  char wave[4];
  std::uint16_t format;
  std::uint16_t channels;
  std::uint32_t rate;
  std::uint16_t bits;
  std::uint32_t data_bytes;
};

std::uint32_t read_u32(const unsigned char* p) {
  return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
         (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

Header read_header(const char* path) {
  unsigned char buf[44]{};
  std::FILE* f = std::fopen(path, "rb");
  if (f != nullptr) {
    (void)std::fread(buf, 1, sizeof buf, f);
    std::fclose(f);
  }
  Header h{};
  std::memcpy(h.riff, buf, 4);
  h.size = read_u32(buf + 4);
  std::memcpy(h.wave, buf + 8, 4);
  h.format = static_cast<std::uint16_t>(buf[20] | (buf[21] << 8));
  h.channels = static_cast<std::uint16_t>(buf[22] | (buf[23] << 8));
  h.rate = read_u32(buf + 24);
  h.bits = static_cast<std::uint16_t>(buf[34] | (buf[35] << 8));
  h.data_bytes = read_u32(buf + 40);
  return h;
}

constexpr const char* kPath = "/tmp/xtrans-test.wav";

}  // namespace

TEST_CASE(test_wav_writes_a_valid_header) {
  const std::vector<std::int16_t> pcm(16000, 1000);
  CHECK_EQ(write_wav(kPath, pcm, 16000), 44U + pcm.size() * 2);

  const Header h = read_header(kPath);
  CHECK_EQ(std::string_view(h.riff, 4), std::string_view("RIFF"));
  CHECK_EQ(std::string_view(h.wave, 4), std::string_view("WAVE"));
  CHECK_EQ(h.format, 1);
  CHECK_EQ(h.channels, 1);
  CHECK_EQ(h.rate, 16000U);
  CHECK_EQ(h.bits, 16);
  CHECK_EQ(h.data_bytes, static_cast<std::uint32_t>(pcm.size() * 2));
  CHECK_EQ(h.size, 36U + h.data_bytes);
}

TEST_CASE(test_wav_handles_empty_and_missing_path) {
  CHECK_EQ(write_wav(kPath, std::span<const std::int16_t>{}, 16000), 44U);
  CHECK_EQ(write_wav("/nonexistent-dir/x.wav", std::span<const std::int16_t>{}, 16000), 0U);
}
