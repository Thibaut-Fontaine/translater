#include "harness.hpp"
#include "xtrans/text.hpp"

#include <array>
#include <string_view>

using namespace xtrans;

TEST_CASE(test_text_appends_segments_with_single_spaces) {
  std::array<char, 64> storage{};
  TextBuffer text{storage};
  CHECK(text.append("hello"));
  CHECK(text.append("world"));
  CHECK_EQ(text.text(), std::string_view("hello world"));
  CHECK(!text.truncated());
}

TEST_CASE(test_text_trims_segments_and_ignores_empty_ones) {
  std::array<char, 64> storage{};
  TextBuffer text{storage};
  CHECK(text.append("  hello \n"));
  CHECK(text.append("   "));
  CHECK(text.append("\r\nworld\t"));
  CHECK_EQ(text.text(), std::string_view("hello world"));
}

TEST_CASE(test_text_truncates_and_flags_it) {
  std::array<char, 8> storage{};
  TextBuffer text{storage};
  CHECK(!text.append("abcdefghijkl"));
  CHECK(text.truncated());
  CHECK_EQ(text.text().size(), storage.size());
  CHECK_EQ(text.text(), std::string_view("abcdefgh"));
}

TEST_CASE(test_text_never_splits_a_utf8_sequence) {
  // "€" is 0xE2 0x82 0xAC: with three bytes left, the last sequence must be
  // dropped rather than truncated mid-code-point.
  std::array<char, 3> storage{};
  TextBuffer text{storage};
  CHECK(!text.append("a€"));  // 1 + 3 bytes; one byte hangs over the edge
  const std::string_view out = text.text();
  CHECK_EQ(out.size(), 1U);
  CHECK_EQ(out, std::string_view("a"));
  CHECK(text.truncated());
}

TEST_CASE(test_text_clear_resets) {
  std::array<char, 16> storage{};
  TextBuffer text{storage};
  text.append("hi");
  text.clear();
  CHECK(text.text().empty());
  CHECK(!text.truncated());
  CHECK(text.append("yo"));
  CHECK_EQ(text.text(), std::string_view("yo"));
}

TEST_CASE(test_text_rejects_a_zero_length_buffer) {
  TextBuffer text{std::span<char>{}};
  CHECK(!text.append("x"));
  CHECK(text.text().empty());
  CHECK(text.truncated());
}
