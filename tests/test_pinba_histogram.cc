#include <gtest/gtest.h>

#include "pinba.h"

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
