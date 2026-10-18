#ifndef PINBA_PERCENTILE_H
#define PINBA_PERCENTILE_H

#include <algorithm>
#include <charconv>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string_view>

struct pinba_percentile {
  double value;
  uint64_t numerator;
  uint64_t denominator;  // zero selects the legacy float/floor rank
};

// Opt-in grammar: decimal percent with at most nine fractional digits. No
// exponent, sign, locale-specific separator, partial token or non-finite input.
inline bool pinba_parse_fractional_percentile(std::string_view text, pinba_percentile &out) {
  if (text.empty() || text.front() < '0' || text.front() > '9' || text.back() == '.') {
    return false;
  }
  uint64_t number = 0, scale = 1;
  unsigned int decimals = 0;
  bool dot = false;
  for (const char c : text) {
    if (c == '.' && !dot) {
      dot = true;
      continue;
    }
    if (c < '0' || c > '9' || (dot && ++decimals > 9)) return false;
    if (dot) scale *= 10;
    number = number * 10 + static_cast<unsigned int>(c - '0');
    if (number > 100 * scale) return false;
  }
  if (number == 0) return false;
  out = {static_cast<double>(number) / scale, number, 100 * scale};
  return true;
}

inline size_t pinba_percentile_rank(size_t count, const pinba_percentile &percentile) {
  if (count == 0) return 0;
  if (percentile.denominator != 0) {
    // GCC/Clang on every supported 64-bit target provide this wide integer.
    // Exact decimal ceil avoids 99.99% landing one rank away due to FP error.
    __extension__ using wide = unsigned __int128;
    const wide product = static_cast<wide>(count) * percentile.numerator;
    return static_cast<size_t>((product + percentile.denominator - 1) / percentile.denominator);
  }
  const float rank = count * (static_cast<float>(percentile.value) / 100);
  if (static_cast<long double>(rank) >= static_cast<long double>(count)) return count;
  return std::max(size_t{1}, static_cast<size_t>(rank));
}

inline bool pinba_parse_histogram_max(std::string_view text, int &out) {
  // Preserve positive decimal truncation from the old strtod-to-int path,
  // while rejecting zero, negative, NaN/Inf and values outside int range.
  while (!text.empty() && text.front() == ' ') text.remove_prefix(1);
  if (!text.empty() && text.front() == '+') text.remove_prefix(1);
  if (text.empty()) return false;
  double value = 0;
  const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
  if (result.ec != std::errc{} || result.ptr != text.data() + text.size() ||
      !std::isfinite(value) || value < 1 || value > INT_MAX)
    return false;
  out = static_cast<int>(value);
  return true;
}

#endif
