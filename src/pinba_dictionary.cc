#include "pinba_dictionary.h"

#include <new>

#include "pinba_map.h"
#include "pinba_runtime.h"

pinba_word *pinba_dictionary_intern_rdlock(void **dictionary, pthread_rwlock_t *lock,
                                           const char *str, size_t length) {
  char key[PINBA_TAG_VALUE_SIZE];
  const size_t copied = std::min(length, sizeof(key) - 1);
  memcpy(key, str, copied);
  key[copied] = '\0';

  auto *word = static_cast<pinba_word *>(pinba_map_get(*dictionary, key));
  if (word) {
    return pinba_word_retain(word);
  }

  pthread_rwlock_unlock(lock);
  const auto wait_started = pinba_now_ns();
  pthread_rwlock_wrlock(lock);
  pinba_count(pinba_metric::words_write_wait_ns, pinba_now_ns() - wait_started);
  word = static_cast<pinba_word *>(pinba_map_get(*dictionary, key));
  if (!word) {
    word = new (std::nothrow) pinba_word{};
    if (word) {
      word->str = strdup(key);
      word->len = copied;
      if (word->str && pinba_map_try_add(dictionary, key, word)) {
        const auto live = pinba_counter(pinba_metric::words_live).fetch_add(1) + 1;
        const auto bytes = pinba_counter(pinba_metric::words_bytes)
                               .fetch_add(sizeof(pinba_word) + 2 * (copied + 1)) +
                           sizeof(pinba_word) + 2 * (copied + 1);
        pinba_count(pinba_metric::words_created);
        pinba_peak(pinba_metric::words_peak, live);
        pinba_peak(pinba_metric::words_bytes_peak, bytes);
      } else {
        free(word->str);
        delete word;
        word = nullptr;
      }
    }
  }
  // Retain before dropping the write lock: the collector may otherwise delete
  // a just-inserted word before we reacquire the read lock.
  pinba_word_retain(word);
  pthread_rwlock_unlock(lock);
  pthread_rwlock_rdlock(lock);
  return word;
}

pinba_word *pinba_dictionary_lookup(void **dictionary, pthread_rwlock_t *lock, const char *str) {
  pthread_rwlock_rdlock(lock);
  auto *word = pinba_word_retain(static_cast<pinba_word *>(pinba_map_get(*dictionary, str)));
  pthread_rwlock_unlock(lock);
  return word;
}

size_t pinba_dictionary_collect(void **dictionary, pthread_rwlock_t *lock) {
  const pinba_metric_duration duration{pinba_metric::gc_ns};
  pinba_count(pinba_metric::gc_runs);
  pthread_rwlock_wrlock(lock);
  const size_t collected = pinba_map_delete_if(*dictionary, [](const void *value) {
    auto *word = static_cast<const pinba_word *>(value);
    if (word->references.load(std::memory_order_acquire) != 0) {
      return false;
    }
    pinba_counter(pinba_metric::words_live).fetch_sub(1);
    pinba_counter(pinba_metric::words_bytes).fetch_sub(sizeof(pinba_word) + 2 * (word->len + 1));
    free(word->str);
    delete word;
    return true;
  });
  pthread_rwlock_unlock(lock);
  pinba_count(pinba_metric::words_collected, collected);
  return collected;
}
