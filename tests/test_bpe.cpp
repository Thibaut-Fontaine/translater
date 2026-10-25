#include "harness.hpp"
#include "xtrans/bpe.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

using namespace xtrans;

namespace {

void put32(std::vector<std::uint8_t>& out, std::uint32_t v) {
  out.push_back(static_cast<std::uint8_t>(v & 0xFFU));
  out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFFU));
  out.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFFU));
  out.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFFU));
}

struct Pair {
  std::string piece;
  std::uint32_t id;
};

struct MergeIn {
  std::uint32_t left;
  std::uint32_t right;
  std::uint32_t result;
  std::uint32_t rank;
};

// Builds the blob by hand: the format is the contract with the generator, so
// the test does not need a 12 MB file to prove the encoder works.
std::vector<std::uint8_t> make_blob(std::vector<Pair> vocab, std::vector<MergeIn> merges,
                                    std::uint32_t unk = 3) {
  std::sort(vocab.begin(), vocab.end(),
            [](const Pair& a, const Pair& b) { return a.piece < b.piece; });
  std::sort(merges.begin(), merges.end(), [](const MergeIn& a, const MergeIn& b) {
    return a.left != b.left ? a.left < b.left : a.right < b.right;
  });

  std::vector<std::uint8_t> blob{'X', 'B', 'P', 'E', '1'};
  put32(blob, 1);
  put32(blob, unk);
  put32(blob, static_cast<std::uint32_t>(vocab.size()));
  for (const auto& entry : vocab) {
    put32(blob, entry.id);
    put32(blob, static_cast<std::uint32_t>(entry.piece.size()));
    blob.insert(blob.end(), entry.piece.begin(), entry.piece.end());
  }
  put32(blob, static_cast<std::uint32_t>(merges.size()));
  for (const auto& m : merges) {
    put32(blob, m.left);
    put32(blob, m.right);
    put32(blob, m.result);
    put32(blob, m.rank);
  }
  return blob;
}

const std::string kMeta = "\xE2\x96\x81";  // U+2581

std::vector<Pair> small_vocab() {
  return {
      {kMeta + "b", 12}, {kMeta + "bo", 13}, {kMeta, 5}, {"b", 6},
      {"o", 7},          {"n", 8},           {"j", 9},
  };
}

std::vector<MergeIn> small_merges() {
  return {
      {5, 6, 12, 0},   // ▁ + b -> ▁b
      {12, 7, 13, 2},  // ▁b + o -> ▁bo
      {6, 7, 14, 1},   // b + o -> bo (unused id on purpose)
  };
}

}  // namespace

TEST_CASE(test_bpe_merges_by_rank_not_by_position) {
  const auto blob = make_blob(small_vocab(), small_merges());
  Bpe bpe;
  CHECK(bpe.load(blob));
  std::int32_t out[8] = {};
  // "bo" becomes ▁ + b + o; rank 0 (▁b) wins over rank 1 (bo), then rank 2.
  CHECK_EQ(bpe.encode("bo", out), 1U);
  CHECK_EQ(out[0], 13);
}

TEST_CASE(test_bpe_starts_every_word_with_the_metaspace_piece) {
  const auto blob = make_blob(small_vocab(), small_merges());
  Bpe bpe;
  CHECK(bpe.load(blob));
  std::int32_t out[8] = {};
  CHECK_EQ(bpe.encode("on", out), 3U);
  CHECK_EQ(out[0], 5);  // ▁, not merged with a following space
  CHECK_EQ(out[1], 7);
  CHECK_EQ(out[2], 8);
}

TEST_CASE(test_bpe_collapses_whitespace_and_spaces_words) {
  const auto blob = make_blob(small_vocab(), small_merges());
  Bpe bpe;
  CHECK(bpe.load(blob));
  std::int32_t out[16] = {};
  // Tabs and runs of spaces are one boundary: ▁on ▁bo.
  CHECK_EQ(bpe.encode("on \t  bo", out), 4U);
  CHECK_EQ(out[0], 5);
  CHECK_EQ(out[1], 7);
  CHECK_EQ(out[2], 8);
  CHECK_EQ(out[3], 13);
}

TEST_CASE(test_bpe_unknown_characters_become_one_unk_per_run) {
  const auto blob = make_blob(small_vocab(), small_merges());
  Bpe bpe;
  CHECK(bpe.load(blob));
  CHECK_EQ(bpe.unk_id(), 3);
  std::int32_t out[8] = {};
  // "é" is not in the vocabulary; two of them fuse into a single <unk>.
  CHECK_EQ(bpe.encode("\xC3\xA9\xC3\xA9on", out), 4U);
  CHECK_EQ(out[0], 5);
  CHECK_EQ(out[1], 3);
  CHECK_EQ(out[2], 7);
  CHECK_EQ(out[3], 8);
}

TEST_CASE(test_bpe_decode_restores_spaces_and_drops_control_tokens) {
  const auto blob = make_blob(small_vocab(), small_merges());
  Bpe bpe;
  CHECK(bpe.load(blob));
  const std::int32_t ids[] = {0, 5, 7, 8, 5, 13, 2};
  char out[32] = {};
  CHECK_EQ(bpe.decode(ids, out), 5U);
  CHECK_EQ(std::string_view(out, 5), std::string_view("on bo"));
}

TEST_CASE(test_bpe_encode_refuses_to_truncate) {
  const auto blob = make_blob(small_vocab(), small_merges());
  Bpe bpe;
  CHECK(bpe.load(blob));
  std::int32_t small[1] = {};
  CHECK_EQ(bpe.encode("on on", small), 0U);
}

TEST_CASE(test_bpe_rejects_a_corrupt_blob) {
  Bpe bpe;
  const std::vector<std::uint8_t> empty;
  CHECK(!bpe.load(empty));

  auto wrong_magic = make_blob(small_vocab(), small_merges());
  wrong_magic[0] = 'Y';
  CHECK(!bpe.load(wrong_magic));

  auto truncated = make_blob(small_vocab(), small_merges());
  truncated.resize(truncated.size() - 4);
  CHECK(!bpe.load(truncated));

  // Nothing is half-loaded: a failed load leaves the encoder unusable.
  std::int32_t out[4] = {};
  CHECK_EQ(bpe.encode("bo", out), 0U);
}
