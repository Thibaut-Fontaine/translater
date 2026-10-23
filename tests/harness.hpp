// Minimal, dependency-free test harness.
// Rules: no exceptions, no RTTI, no allocation, deterministic order.
#pragma once

#include <cstdio>
#include <string_view>
#include <type_traits>

namespace t {

inline int& failures() noexcept {
  static int n = 0;
  return n;
}

struct Case {
  const char* name;
  void (*fn)();
  Case* next;
};

inline Case*& list() noexcept {
  static Case* head = nullptr;
  return head;
}

struct Reg {
  Case case_;
  Reg(const char* name, void (*fn)()) : case_{name, fn, list()} { list() = &case_; }
};

inline void fail(const char* file, int line, const char* expr) noexcept {
  ++failures();
  std::fprintf(stderr, "  FAIL %s:%d: %s\n", file, line, expr);
}

template <class T>
void show(const T& v) noexcept {
  if constexpr (std::is_same_v<T, bool>) {
    std::fputs(v ? "true" : "false", stderr);
  } else if constexpr (std::is_enum_v<T>) {
    std::fprintf(stderr, "%d", static_cast<int>(v));
  } else if constexpr (std::is_integral_v<T>) {
    std::fprintf(stderr, "%lld", static_cast<long long>(v));
  } else if constexpr (std::is_floating_point_v<T>) {
    std::fprintf(stderr, "%.7g", static_cast<double>(v));
  } else if constexpr (std::is_convertible_v<T, std::string_view>) {
    const std::string_view s = v;
    std::fprintf(stderr, "%.*s", static_cast<int>(s.size()), s.data());
  } else {
    std::fputs("<?>", stderr);
  }
}

template <class A, class B>
void check_eq(const A& a, const B& b, const char* ea, const char* eb, const char* file,
              int line) noexcept {
  if (a == b) return;
  ++failures();
  std::fprintf(stderr, "  FAIL %s:%d: %s == %s (left=", file, line, ea, eb);
  show(a);
  std::fputs(", right=", stderr);
  show(b);
  std::fputs(")\n", stderr);
}

inline int run_all() noexcept {
  int ran = 0;
  for (const Case* c = list(); c != nullptr; c = c->next) {
    const int before = failures();
    std::fprintf(stderr, "[ RUN  ] %s\n", c->name);
    c->fn();
    ++ran;
    if (failures() != before) std::fprintf(stderr, "[ FAIL ] %s\n", c->name);
  }
  std::fprintf(stderr, "\n%d tests, %d failed assertions\n", ran, failures());
  return failures() == 0 ? 0 : 1;
}

}  // namespace t

#define TEST_CASE(name)                       \
  static void name();                         \
  static const ::t::Reg reg_##name(#name, name); \
  static void name()

#define CHECK(expr)                                    \
  do {                                                 \
    if (!(expr)) ::t::fail(__FILE__, __LINE__, #expr);  \
  } while (false)

#define CHECK_EQ(a, b) ::t::check_eq((a), (b), #a, #b, __FILE__, __LINE__)
