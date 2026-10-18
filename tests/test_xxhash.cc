#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>

#include "xxhash.h"

namespace {

// Frozen from the existing implementation before changing its memory loads.
// The input includes zero bytes and crosses both 16/32-byte block boundaries.
struct HashVector {
  size_t length;
  unsigned int seed;
  unsigned int hash32;
  unsigned long long hash64;
};

constexpr HashVector vectors[] = {
    {0, 0, 0x02cc5d05U, 0xef46db3751d8e999ULL},   {0, 2001, 0xf1a8b8beU, 0xc931f870d4714b76ULL},
    {1, 0, 0x3fe5a29dU, 0xf592c0c7639c4cb6ULL},   {1, 2001, 0xc3090738U, 0x5bc9399137d995cdULL},
    {3, 0, 0x4021af28U, 0x22c08528601d4f27ULL},   {3, 2001, 0xaea39f69U, 0xdd2b4f027cba0799ULL},
    {15, 0, 0x162986ebU, 0x90a9714eb00e8d29ULL},  {15, 2001, 0xa8418909U, 0x9d7869cbe4ad7a37ULL},
    {16, 0, 0x96ac9fa6U, 0xc6843cef99721174ULL},  {16, 2001, 0x5e36c0c4U, 0xf378c332675bdc81ULL},
    {17, 0, 0x3cdfb1a2U, 0xf72e5e622b272ad6ULL},  {17, 2001, 0xd227260aU, 0x0e39744cf0526011ULL},
    {31, 0, 0xdf853fbfU, 0xe4a0e629e519a4aeULL},  {31, 2001, 0x9fe98b05U, 0x2dd660f40bd1f873ULL},
    {32, 0, 0xda8072dcU, 0xcc6b8aaada790b2dULL},  {32, 2001, 0xdea1673aU, 0xa1f21993d33557dcULL},
    {33, 0, 0x3c8fd07dU, 0x35ec49850475a832ULL},  {33, 2001, 0x450dd702U, 0x55b0d701209db4e5ULL},
    {63, 0, 0x00bf10c5U, 0xbf9f0ba3cf95b28aULL},  {63, 2001, 0x91538d15U, 0xe41ecefd1313c829ULL},
    {64, 0, 0x043913c6U, 0x155ccce4bf32befcULL},  {64, 2001, 0xd0e455e6U, 0xf41be4a735ac8813ULL},
    {65, 0, 0x3922f755U, 0xda5ee441a595cfb8ULL},  {65, 2001, 0x4d6a23d9U, 0x04535fcafbddf9cdULL},
    {127, 0, 0x76a8c2e8U, 0xf648961449461a69ULL}, {127, 2001, 0x2bb9edf8U, 0xddb04ece57b16744ULL},
    {256, 0, 0x8021d20cU, 0x43c92f09cb3e28cfULL}, {256, 2001, 0xb9e74c17U, 0x18e3cbb7f2481c93ULL},
};

TEST(XxHash, PreservesLegacyHashesAtEveryAlignment) {
  alignas(8) std::array<unsigned char, 256 + 8> input{};
  for (const auto &vector : vectors) {
    for (size_t offset = 0; offset < 8; offset++) {
      SCOPED_TRACE(::testing::Message() << vector.length << "/" << vector.seed << "/" << offset);
      for (size_t i = 0; i < vector.length; i++) {
        input[offset + i] = static_cast<unsigned char>(i * 37 + 11);
      }
      EXPECT_EQ(XXH32(input.data() + offset, vector.length, vector.seed), vector.hash32);
      EXPECT_EQ(XXH64(input.data() + offset, vector.length, vector.seed), vector.hash64);
    }
  }
}

TEST(XxHash, EveryShortLengthAndSeedIsIndependentOfAlignment) {
  alignas(8) std::array<unsigned char, 256> aligned{};
  for (size_t i = 0; i < aligned.size(); i++) {
    aligned[i] = static_cast<unsigned char>(i * 37 + 11);
  }
  for (const unsigned long long seed : {0ULL, 2001ULL, 0xffffffffffffffffULL}) {
    for (size_t length = 0; length <= aligned.size(); length++) {
      const auto hash32 = XXH32(aligned.data(), length, static_cast<unsigned int>(seed));
      const auto hash64 = XXH64(aligned.data(), length, seed);
      for (size_t offset = 0; offset < 8; offset++) {
        // Exact allocation also lets ASan detect reads beyond the input tail.
        auto input = std::make_unique<unsigned char[]>(std::max(size_t{1}, length + offset));
        memcpy(input.get() + offset, aligned.data(), length);
        EXPECT_EQ(XXH32(input.get() + offset, length, static_cast<unsigned int>(seed)), hash32);
        EXPECT_EQ(XXH64(input.get() + offset, length, seed), hash64);
      }
    }
  }
}

TEST(XxHash, StreamingAcrossBlockAndTailBoundariesPreservesLegacyHashes) {
  alignas(8) std::array<unsigned char, 256 + 8> input{};
  for (const auto &vector : vectors) {
    for (size_t offset = 0; offset < 8; offset++) {
      for (size_t i = 0; i < vector.length; i++) {
        input[offset + i] = static_cast<unsigned char>(i * 37 + 11);
      }
      for (const size_t chunk : {1, 3, 7, 15, 16, 17, 31, 32, 33}) {
        XXH32_state_t state32{};
        XXH64_state_t state64{};
        ASSERT_EQ(XXH32_reset(&state32, vector.seed), XXH_OK);
        ASSERT_EQ(XXH64_reset(&state64, vector.seed), XXH_OK);
        for (size_t pos = 0; pos < vector.length; pos += chunk) {
          const size_t count = std::min(chunk, vector.length - pos);
          ASSERT_EQ(XXH32_update(&state32, input.data() + offset + pos, count), XXH_OK);
          ASSERT_EQ(XXH64_update(&state64, input.data() + offset + pos, count), XXH_OK);
        }
        EXPECT_EQ(XXH32_digest(&state32), vector.hash32);
        EXPECT_EQ(XXH64_digest(&state64), vector.hash64);
      }
    }
  }
}

}  // namespace
