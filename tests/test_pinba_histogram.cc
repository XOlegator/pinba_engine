#include <gtest/gtest.h>

#include "pinba.h"
#include "pinba_percentile.h"

// Exercise the actual inline updater, including its weighted add/delete path.
pinba_daemon *D = nullptr;

namespace {

class PinbaHistogram : public ::testing::Test {
 protected:
  pinba_daemon daemon{};
  pinba_std_report report{};
  void *histogram = nullptr;

  void SetUp() override {
    D = &daemon;
    daemon.settings.histogram_size = 1024;
    report.histogram_max_time = 10;
    report.histogram_segment = 10.0f / 1024;
  }

  void TearDown() override {
    pinba_lmap_destroy(histogram);
    D = nullptr;
  }

  size_t count(unsigned int bucket) {
    return reinterpret_cast<size_t>(pinba_lmap_get(histogram, bucket));
  }
};

TEST_F(PinbaHistogram, UpperBoundaryAndOverflowStayInLastBucket) {
  for (const pinba_timeval sample : {pinba_timeval{9, 999999}, pinba_timeval{10, 0},
                                     pinba_timeval{10, 1}, pinba_timeval{100, 0}}) {
    pinba_update_histogram(&report, &histogram, &sample, 1);
  }
  EXPECT_EQ(count(1023), 4u);
  EXPECT_EQ(count(0), 0u);
}

TEST_F(PinbaHistogram, WeightedUpperBoundaryCanBeAddedAndRemoved) {
  const pinba_timeval sample{50, 0};  // five hits, ten seconds per hit
  pinba_update_histogram(&report, &histogram, &sample, 5);
  EXPECT_EQ(count(1023), 5u);
  EXPECT_EQ(count(0), 0u);
  pinba_update_histogram(&report, &histogram, &sample, -5);
  EXPECT_EQ(pinba_lmap_count(histogram), 0u);
}

TEST_F(PinbaHistogram, ZeroAndInteriorBucketsAreUnchanged) {
  for (const pinba_timeval sample : {pinba_timeval{0, 0}, pinba_timeval{5, 0}}) {
    pinba_update_histogram(&report, &histogram, &sample, 1);
  }
  EXPECT_EQ(count(0), 1u);
  EXPECT_EQ(count(512), 1u);
  EXPECT_EQ(count(1023), 0u);
}

}  // namespace

TEST(PinbaPercentiles, ExactDecimalRanksAndStrictGrammar) {
  pinba_percentile percentile{};
  ASSERT_TRUE(pinba_parse_fractional_percentile("99.9", percentile));
  EXPECT_EQ(pinba_percentile_rank(100000, percentile), 99900u);
  ASSERT_TRUE(pinba_parse_fractional_percentile("99.99", percentile));
  EXPECT_EQ(pinba_percentile_rank(100000, percentile), 99990u);
  EXPECT_EQ(pinba_percentile_rank(3, percentile), 3u);
  EXPECT_EQ(pinba_percentile_rank(0, percentile), 0u);
  for (const char *invalid : {"", "NaN", "inf", "99e0", "99,9", "99.9x", " 99.9", "+99", ".9",
                              "99.", "0", "100.01", "99.1234567890"}) {
    EXPECT_FALSE(pinba_parse_fractional_percentile(invalid, percentile)) << invalid;
  }
  ASSERT_TRUE(pinba_parse_fractional_percentile("100", percentile));
  EXPECT_EQ(pinba_percentile_rank(SIZE_MAX, percentile), SIZE_MAX);
}

TEST(PinbaPercentiles, LegacyFloatFloorRanksRemainUnchanged) {
  for (size_t count : {0u, 1u, 3u, 100u, 100000u}) {
    for (int percent : {1, 25, 90, 95, 99, 100}) {
      const auto legacy = static_cast<unsigned int>(count * (static_cast<float>(percent) / 100));
      EXPECT_EQ(pinba_percentile_rank(count, {static_cast<double>(percent), 0, 0}),
                count == 0 ? 0u : std::max(1u, legacy));
    }
  }
}

TEST(PinbaPercentiles, RejectsHistogramConfigThatWouldCreateInvalidBuckets) {
  int maximum = 0;
  for (const char *invalid :
       {"", "s10", "0", "-1", "0.5", "0s", "NaN", "inf", "1e999", "2147483648"}) {
    EXPECT_FALSE(pinba_parse_histogram_max(invalid, maximum)) << invalid;
  }
  ASSERT_TRUE(pinba_parse_histogram_max("10.5", maximum));
  EXPECT_EQ(maximum, 10);
  // Legacy strtod prefix parsing: existing COMMENT suffixes keep opening.
  ASSERT_TRUE(pinba_parse_histogram_max(" 10s", maximum));
  EXPECT_EQ(maximum, 10);
}
