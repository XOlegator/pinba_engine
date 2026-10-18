#ifndef PINBA_REQUEST_VALIDATION_H
#define PINBA_REQUEST_VALIDATION_H

#include <climits>

#include "pinba.pb-c.h"

enum class pinba_request_error {
  none,
  timer_arrays,
  flattened_tags,
  dictionary_index,
  representation_limit,
  count
};

inline size_t pinba_stored_timer_count(const Pinba__Request &request) {
  size_t count = 0;
  for (size_t i = 0; i < request.n_timer_hit_count; i++) {
    count += request.timer_hit_count[i] != 0 && request.timer_tag_count[i] != 0;
  }
  return count;
}

// Validate before interning strings or indexing any flattened array. Numeric
// sanitation and optional/partial CPU arrays retain their legacy semantics.
inline pinba_request_error pinba_validate_request(const Pinba__Request &request) {
  const size_t timers = request.n_timer_hit_count;
  if (timers != request.n_timer_value || timers != request.n_timer_tag_count ||
      request.n_tag_name != request.n_tag_value) {
    return pinba_request_error::timer_arrays;
  }
  if (timers > USHRT_MAX || request.n_tag_name > UINT_MAX) {
    return pinba_request_error::representation_limit;
  }
  if (request.n_timer_tag_name != request.n_timer_tag_value) {
    return pinba_request_error::flattened_tags;
  }
  if (request.n_dictionary == 0 && (timers != 0 || request.n_tag_name != 0)) {
    return pinba_request_error::dictionary_index;
  }
  size_t total = 0;
  for (size_t i = 0; i < timers; i++) {
    const size_t tags = request.timer_tag_count[i];
    if (tags > USHRT_MAX || request.timer_hit_count[i] > INT_MAX) {
      return pinba_request_error::representation_limit;
    }
    // Subtraction bounds the addition as well as rejecting short arrays.
    if (tags > request.n_timer_tag_name - total) {
      return pinba_request_error::flattened_tags;
    }
    total += tags;
  }
  if (total != request.n_timer_tag_name) {
    return pinba_request_error::flattened_tags;
  }
  for (size_t i = 0; i < total; i++) {
    if (request.timer_tag_name[i] >= request.n_dictionary ||
        request.timer_tag_value[i] >= request.n_dictionary) {
      return pinba_request_error::dictionary_index;
    }
  }
  for (size_t i = 0; i < request.n_tag_name; i++) {
    if (request.tag_name[i] >= request.n_dictionary ||
        request.tag_value[i] >= request.n_dictionary) {
      return pinba_request_error::dictionary_index;
    }
  }
  return pinba_request_error::none;
}

#endif
