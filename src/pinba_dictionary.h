#ifndef PINBA_DICTIONARY_H
#define PINBA_DICTIONARY_H

#include <cassert>
#include <memory>

#include "pinba.h"

// Every stored pointer has an owner. Dropping the final reference does not free
// the word: collection holds words_lock exclusively, so lookup and retain are
// indivisible with respect to reclamation.
inline pinba_word *pinba_word_retain(pinba_word *word) {
  if (word) {
    word->references.fetch_add(1, std::memory_order_relaxed);
  }
  return word;
}

inline void pinba_word_release(pinba_word *word) {
  if (word) {
    [[maybe_unused]] const size_t previous =
        word->references.fetch_sub(1, std::memory_order_release);
    assert(previous > 0);
  }
}

inline void pinba_words_release(pinba_word **words, size_t count) {
  for (size_t i = 0; i < count; i++) {
    pinba_word_release(words[i]);
    words[i] = nullptr;
  }
}

struct pinba_word_releaser {
  void operator()(pinba_word *word) const { pinba_word_release(word); }
};
using pinba_word_owner = std::unique_ptr<pinba_word, pinba_word_releaser>;

struct pinba_words_releaser {
  size_t count;
  void operator()(pinba_word **words) const {
    pinba_words_release(words, count);
    free(words);
  }
};
using pinba_words_owner = std::unique_ptr<pinba_word *, pinba_words_releaser>;

// Caller holds a read lock on entry and return. Returns one owned reference.
struct pinba_dictionary_budget {
  uint64_t max_words = 0;
  uint64_t max_bytes = 0;
};
pinba_word *pinba_dictionary_intern_rdlock(void **dictionary, pthread_rwlock_t *lock,
                                           const char *str, size_t length,
                                           pinba_dictionary_budget budget = {});
// Acquires its own lock and returns one owned reference, or nullptr if absent.
pinba_word *pinba_dictionary_lookup(void **dictionary, pthread_rwlock_t *lock, const char *str);
// No other engine lock is needed: referenced words remain alive through owners.
size_t pinba_dictionary_collect(void **dictionary, pthread_rwlock_t *lock);

inline void pinba_record_release_words(pinba_stats_record *record) {
  pinba_words_release(record->dictionary_words, record->dictionary_words_count);
  record->dictionary_words_count = 0;
  record->data.tags_cnt = 0;
}

inline void pinba_tmp_release_words(pinba_stats_record_ex *record) {
  pinba_words_release(record->words, record->words_cnt);
  record->words_cnt = 0;
  record->record.data.tags_cnt = 0;
}

#endif
