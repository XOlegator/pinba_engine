#include <gtest/gtest.h>

#include <atomic>
#include <string>
#include <thread>
#include <vector>

#include "pinba_dictionary.h"
#include "pinba_map.h"
#include "pinba_runtime.h"

namespace {

class PinbaDictionary : public ::testing::Test {
 protected:
  void *dictionary = nullptr;
  pthread_rwlock_t lock{};

  void SetUp() override { ASSERT_EQ(pthread_rwlock_init(&lock, nullptr), 0); }

  void TearDown() override {
    pinba_dictionary_collect(&dictionary, &lock);
    EXPECT_EQ(pinba_map_count(dictionary), 0u);
    pinba_map_destroy(dictionary);
    pthread_rwlock_destroy(&lock);
  }

  pinba_word *intern(const std::string &value, pinba_dictionary_budget budget = {}) {
    pthread_rwlock_rdlock(&lock);
    auto *word =
        pinba_dictionary_intern_rdlock(&dictionary, &lock, value.c_str(), value.size(), budget);
    pthread_rwlock_unlock(&lock);
    return word;
  }
};

TEST_F(PinbaDictionary, ReusesIdentityUntilLastOwnerReleases) {
  pinba_word_owner first(intern("shared"));
  pinba_word_owner second(intern("shared"));
  ASSERT_EQ(first.get(), second.get());
  EXPECT_EQ(first->references.load(), 2u);
  first.reset();
  EXPECT_EQ(pinba_dictionary_collect(&dictionary, &lock), 0u);
  EXPECT_STREQ(second->str, "shared");
  second.reset();
  EXPECT_EQ(pinba_dictionary_collect(&dictionary, &lock), 1u);
}

TEST_F(PinbaDictionary, RawRequestOwnsBorrowedRequestTagsAndTimerValues) {
  pinba_word *temporary_words[] = {intern("group"), intern("value"), intern("value")};
  pinba_stats_record_ex temporary{};
  temporary.words = temporary_words;
  temporary.words_cnt = 3;
  pinba_word *raw_words[3];
  for (size_t i = 0; i < 3; i++) {
    raw_words[i] = pinba_word_retain(temporary_words[i]);
  }
  pinba_stats_record raw{};
  raw.dictionary_words = raw_words;
  raw.dictionary_words_count = 3;
  pinba_word *tags[] = {raw_words[0]};
  pinba_word *values[] = {raw_words[1]};
  raw.data.tag_names = tags;
  raw.data.tag_values = values;
  raw.data.tags_cnt = 1;
  pinba_timer_record timer{};
  timer.tag_values = values;  // borrowed from the parent's dictionary

  pinba_tmp_release_words(&temporary);
  EXPECT_EQ(temporary.words_cnt, 0u);
  EXPECT_EQ(pinba_dictionary_collect(&dictionary, &lock), 0u);
  EXPECT_STREQ(raw.data.tag_names[0]->str, "group");
  EXPECT_STREQ(timer.tag_values[0]->str, "value");
  pinba_record_release_words(&raw);
  EXPECT_EQ(raw.data.tags_cnt, 0u);
  EXPECT_EQ(pinba_dictionary_collect(&dictionary, &lock), 2u);
}

TEST_F(PinbaDictionary, ReportSelectorSurvivesRawExpirationAndCanBeReleased) {
  pinba_word_owner raw(intern("selector"));
  pinba_word_owner report(pinba_dictionary_lookup(&dictionary, &lock, "selector"));
  ASSERT_EQ(raw.get(), report.get());
  raw.reset();
  EXPECT_EQ(pinba_dictionary_collect(&dictionary, &lock), 0u);
  EXPECT_STREQ(report->str, "selector");
  report.reset();
  EXPECT_EQ(pinba_dictionary_collect(&dictionary, &lock), 1u);
  EXPECT_EQ(pinba_dictionary_lookup(&dictionary, &lock, "selector"), nullptr);
}

TEST_F(PinbaDictionary, ReclaimsHighCardinalityChurnAcrossRepeatedWindows) {
  for (int window = 0; window < 20; window++) {
    std::vector<pinba_word_owner> words;
    for (int i = 0; i < 200; i++) {
      words.emplace_back(intern(std::to_string(window) + ":" + std::to_string(i)));
    }
    EXPECT_EQ(pinba_dictionary_collect(&dictionary, &lock), 0u);
    EXPECT_EQ(pinba_map_count(dictionary), 200u);
    words.clear();
    EXPECT_EQ(pinba_dictionary_collect(&dictionary, &lock), 200u);
    EXPECT_EQ(pinba_map_count(dictionary), 0u);
  }
}

TEST_F(PinbaDictionary, PreservesLegacyByteTruncationAndEmptyStrings) {
  const std::string prefix(PINBA_TAG_VALUE_SIZE - 1, 'x');
  pinba_word_owner first(intern(prefix + "first"));
  pinba_word_owner second(intern(prefix + "second"));
  ASSERT_EQ(first.get(), second.get());
  EXPECT_EQ(first->len, prefix.size());
  EXPECT_STREQ(first->str, prefix.c_str());
  pinba_word_owner empty(intern(""));
  EXPECT_EQ(empty->len, 0u);
  EXPECT_STREQ(empty->str, "");
}

TEST_F(PinbaDictionary, PartialSelectorArrayReleasesOnlyInitializedWords) {
  {
    pinba_words_owner words((pinba_word **)calloc(3, sizeof(pinba_word *)),
                            pinba_words_releaser{3});
    ASSERT_NE(words.get(), nullptr);
    words.get()[0] = intern("first");
    words.get()[1] = intern("second");
  }
  EXPECT_EQ(pinba_dictionary_collect(&dictionary, &lock), 2u);
}

TEST_F(PinbaDictionary, ConcurrentInternLookupReleaseAndCollection) {
  std::atomic<bool> finished{false};
  std::thread collector([&] {
    while (!finished.load()) {
      pinba_dictionary_collect(&dictionary, &lock);
      std::this_thread::yield();
    }
  });
  std::vector<std::thread> workers;
  for (int worker = 0; worker < 4; worker++) {
    workers.emplace_back([&, worker] {
      for (int i = 0; i < 500; i++) {
        pinba_word_owner shared(intern("shared"));
        pinba_word_owner unique(intern(std::to_string(worker) + ":" + std::to_string(i)));
        pinba_word_owner lookup(pinba_dictionary_lookup(&dictionary, &lock, "shared"));
        EXPECT_EQ(shared.get(), lookup.get());
        EXPECT_STREQ(shared->str, "shared");
      }
    });
  }
  for (auto &worker : workers) {
    worker.join();
  }
  finished.store(true);
  collector.join();
}

}  // namespace

TEST(PinbaRuntime, KeepsNativeWidthCountersAndConcurrentHighWatermarks) {
  pinba_runtime_reset();
  const uint64_t large = uint64_t{1} << 40;
  pinba_count(pinba_metric::udp_bytes, large);
  EXPECT_EQ(pinba_count_get(pinba_metric::udp_bytes), large);
  std::vector<std::thread> workers;
  for (int i = 0; i < 4; i++) {
    workers.emplace_back([&] {
      for (int n = 0; n < 1000; n++) {
        const auto value = pinba_counter(pinba_metric::queue_live).fetch_add(1) + 1;
        pinba_peak(pinba_metric::queue_peak, value);
      }
    });
  }
  for (auto &worker : workers) worker.join();
  EXPECT_EQ(pinba_count_get(pinba_metric::queue_peak), 4000u);
  pinba_peak(pinba_metric::queue_peak, 1);
  EXPECT_EQ(pinba_count_get(pinba_metric::queue_peak), 4000u);
}

TEST_F(PinbaDictionary, AdmissionLimitsKeepExistingIdentityAndRecoverAfterCollection) {
  pinba_word_owner first(intern("first", {1, 0}));
  ASSERT_NE(first.get(), nullptr);
  pinba_word_owner same(intern("first", {1, 1}));
  EXPECT_EQ(first.get(), same.get());
  EXPECT_EQ(intern("other", {1, 0}), nullptr);
  first.reset();
  same.reset();
  EXPECT_EQ(pinba_dictionary_collect(&dictionary, &lock), 1u);
  pinba_word_owner next(intern("other", {1, 0}));
  EXPECT_NE(next.get(), nullptr);
}

TEST_F(PinbaDictionary, KnownByteBudgetChecksBoundaryAndAvoidsUnsignedUnderflow) {
  const uint64_t charge = sizeof(pinba_word) + 2 * (5 + 1);
  EXPECT_EQ(intern("first", {0, charge - 1}), nullptr);
  pinba_word_owner first(intern("first", {0, charge}));
  ASSERT_NE(first.get(), nullptr);
  EXPECT_EQ(intern("other", {0, charge}), nullptr);
  pinba_word_owner same(intern("first", {0, 1}));
  EXPECT_EQ(first.get(), same.get());
}
