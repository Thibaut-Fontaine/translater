#include "xtrans/bpe.hpp"

#include <algorithm>
#include <cstring>

namespace xtrans {
namespace {

constexpr const char kMagic[5] = {'X', 'B', 'P', 'E', '1'};
constexpr std::uint32_t kVersion = 1;
constexpr std::int32_t kFirstRegularId = 4;  // <s>, <pad>, </s>, <unk>
constexpr std::int32_t kNllbVocab = 256000;
constexpr char kMetaspace[] = "\xE2\x96\x81";  // U+2581

std::uint32_t read_u32(const std::uint8_t* p) noexcept {
  return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
         (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

std::uint16_t read_u16(const std::uint8_t* p) noexcept {
  return static_cast<std::uint16_t>(static_cast<std::uint16_t>(p[0]) |
                                    (static_cast<std::uint16_t>(p[1]) << 8));
}

// Length of the UTF-8 sequence starting at text[i]; 1 for invalid bytes so a
// corrupt transcript degrades to <unk> instead of reading past the end.
std::size_t utf8_length(std::string_view text, std::size_t i) noexcept {
  const auto c = static_cast<unsigned char>(text[i]);
  std::size_t n = 1;
  if ((c & 0xE0) == 0xC0) {
    n = 2;
  } else if ((c & 0xF0) == 0xE0) {
    n = 3;
  } else if ((c & 0xF8) == 0xF0) {
    n = 4;
  }
  if (i + n > text.size()) return 1;
  return n;
}

bool is_space(char c) noexcept {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

void append_byte(std::span<char> out, std::size_t& used, char c) noexcept {
  if (used < out.size()) out[used++] = c;
}

}  // namespace

bool Bpe::load(std::span<const std::uint8_t> blob) noexcept {
  entries_.clear();
  by_id_.clear();
  merges_.clear();
  blob_ = {};

  const auto fail = [this] {
    entries_.clear();
    by_id_.clear();
    merges_.clear();
    blob_ = {};
    return false;
  };

  if (blob.size() < 5 + 4 * 4) return fail();
  if (std::memcmp(blob.data(), kMagic, sizeof(kMagic)) != 0) return fail();

  std::size_t pos = sizeof(kMagic);
  const std::uint32_t version = read_u32(blob.data() + pos);
  pos += 4;
  if (version != kVersion) return fail();
  unk_ = static_cast<std::int32_t>(read_u32(blob.data() + pos));
  pos += 4;

  const std::uint32_t count = read_u32(blob.data() + pos);
  pos += 4;
  entries_.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    if (pos + 8 > blob.size()) return fail();
    const std::uint32_t id = read_u32(blob.data() + pos);
    const std::uint16_t len = read_u16(blob.data() + pos + 4);
    pos += 8;
    if (pos + len > blob.size()) return fail();
    entries_.push_back(Entry{static_cast<std::uint32_t>(pos), len, id});
    pos += len;
  }

  if (pos + 4 > blob.size()) return fail();
  const std::uint32_t merge_count = read_u32(blob.data() + pos);
  pos += 4;
  merges_.reserve(merge_count);
  for (std::uint32_t i = 0; i < merge_count; ++i) {
    if (pos + 16 > blob.size()) return fail();
    merges_.push_back(Merge{read_u32(blob.data() + pos), read_u32(blob.data() + pos + 4),
                            read_u32(blob.data() + pos + 8), read_u32(blob.data() + pos + 12)});
    pos += 16;
  }

  blob_ = blob;
  // Ids are dense in the published vocabulary but a hand-built table may skip
  // values, so size the reverse index by the largest id seen.
  std::uint32_t max_id = 0;
  for (const auto& entry : entries_) max_id = std::max(max_id, entry.id);
  by_id_.assign(static_cast<std::size_t>(max_id) + 1, 0);
  for (std::uint32_t i = 0; i < count; ++i) by_id_[entries_[i].id] = i;
  return true;
}

const Bpe::Entry* Bpe::find_piece(std::string_view text) const noexcept {
  std::size_t lo = 0;
  std::size_t hi = entries_.size();
  while (lo < hi) {
    const std::size_t mid = lo + (hi - lo) / 2;
    const std::string_view probe = piece(entries_[mid]);
    if (probe < text) {
      lo = mid + 1;
    } else if (text < probe) {
      hi = mid;
    } else {
      return &entries_[mid];
    }
  }
  return nullptr;
}

const Bpe::Merge* Bpe::find_merge(std::int32_t left, std::int32_t right) const noexcept {
  const std::uint64_t key = (static_cast<std::uint64_t>(static_cast<std::uint32_t>(left)) << 32) |
                            static_cast<std::uint32_t>(right);
  std::size_t lo = 0;
  std::size_t hi = merges_.size();
  while (lo < hi) {
    const std::size_t mid = lo + (hi - lo) / 2;
    const std::uint64_t probe =
        (static_cast<std::uint64_t>(merges_[mid].left) << 32) | merges_[mid].right;
    if (probe < key) {
      lo = mid + 1;
    } else if (key < probe) {
      hi = mid;
    } else {
      return &merges_[mid];
    }
  }
  return nullptr;
}

std::size_t Bpe::encode(std::string_view text, std::span<std::int32_t> out) noexcept {
  if (!loaded() || out.empty()) return 0;

  std::vector<std::int32_t> symbols;
  symbols.reserve(text.size() + 1);

  const auto push = [&](std::string_view bytes) {
    const Entry* entry = find_piece(bytes);
    symbols.push_back(entry != nullptr ? static_cast<std::int32_t>(entry->id) : unk_);
  };

  push(std::string_view(kMetaspace, 3));
  bool seen = false;
  bool space = false;
  for (std::size_t i = 0; i < text.size();) {
    if (is_space(text[i])) {
      space = true;
      ++i;
      continue;
    }
    if (space && seen) push(std::string_view(kMetaspace, 3));
    space = false;
    const std::size_t n = utf8_length(text, i);
    push(text.substr(i, n));
    i += n;
    seen = true;
  }

  while (symbols.size() > 1) {
    const Merge* best = nullptr;
    for (std::size_t i = 0; i + 1 < symbols.size(); ++i) {
      const Merge* candidate = find_merge(symbols[i], symbols[i + 1]);
      if (candidate != nullptr && (best == nullptr || candidate->rank < best->rank)) {
        best = candidate;
      }
    }
    if (best == nullptr) break;

    std::vector<std::int32_t> merged;
    merged.reserve(symbols.size());
    for (std::size_t i = 0; i < symbols.size();) {
      if (i + 1 < symbols.size() && symbols[i] == static_cast<std::int32_t>(best->left) &&
          symbols[i + 1] == static_cast<std::int32_t>(best->right)) {
        merged.push_back(static_cast<std::int32_t>(best->result));
        i += 2;
      } else {
        merged.push_back(symbols[i]);
        ++i;
      }
    }
    symbols.swap(merged);
  }

  if (symbols.size() > out.size()) return 0;

  std::size_t written = 0;
  for (std::size_t i = 0; i < symbols.size(); ++i) {
    if (symbols[i] == unk_ && written > 0 && out[written - 1] == unk_) continue;  // fuse_unk
    out[written++] = symbols[i];
  }
  return written;
}

std::size_t Bpe::decode(std::span<const std::int32_t> ids, std::span<char> out) noexcept {
  if (!loaded() || out.empty()) return 0;

  std::size_t used = 0;
  bool pending_space = false;
  bool started = false;
  for (const std::int32_t id : ids) {
    if (id < kFirstRegularId || id >= kNllbVocab) continue;
    const auto index = static_cast<std::size_t>(id);
    if (index >= by_id_.size()) continue;
    const std::string_view bytes = piece(entries_[by_id_[index]]);
    for (std::size_t i = 0; i < bytes.size();) {
      if (bytes.compare(i, 3, kMetaspace) == 0) {
        if (started) pending_space = true;
        i += 3;
        continue;
      }
      if (pending_space) {
        append_byte(out, used, ' ');
        pending_space = false;
      }
      append_byte(out, used, bytes[i]);
      started = true;
      ++i;
    }
  }
  return used;
}

}  // namespace xtrans
