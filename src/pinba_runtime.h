#ifndef PINBA_RUNTIME_H
#define PINBA_RUNTIME_H

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>

// Names and units are part of the additive SHOW GLOBAL STATUS interface.
#define PINBA_RUNTIME_METRICS(X)                               \
  X(udp_received, udp_datagrams_received)                      \
  X(udp_bytes, udp_bytes_received)                             \
  X(udp_enqueued, udp_datagrams_enqueued)                      \
  X(udp_completed, udp_datagrams_completed)                    \
  X(udp_decode_ns, udp_decode_ns)                              \
  X(udp_decode_errors, udp_decode_errors)                      \
  X(udp_queue_drops, udp_queue_drops)                          \
  X(udp_kernel_drops, udp_kernel_drops)                        \
  X(requests_checked, requests_checked)                        \
  X(requests_invalid, requests_invalid)                        \
  X(bad_timer_arrays, invalid_timer_arrays)                    \
  X(bad_flattened_tags, invalid_flattened_tags)                \
  X(bad_dictionary_index, invalid_dictionary_index)            \
  X(bad_representation, invalid_representation)                \
  X(requests_valid, requests_validated)                        \
  X(requests_staged, requests_staged)                          \
  X(stage_rejects, requests_stage_rejected)                    \
  X(resource_rejects, requests_resource_rejected)              \
  X(raw_admitted, requests_admitted)                           \
  X(raw_capacity_drops, requests_capacity_dropped)             \
  X(raw_allocation_drops, requests_allocation_dropped)         \
  X(timers_admitted, timers_admitted)                          \
  X(timers_skipped, timers_skipped)                            \
  X(timer_allocation_drops, timers_allocation_dropped)         \
  X(timer_tag_allocation_drops, timer_tags_allocation_dropped) \
  X(raw_expired, requests_expired)                             \
  X(expiration_deferred, expiration_deferred_batches)          \
  X(words_live, dictionary_words_live)                         \
  X(words_bytes, dictionary_bytes_known)                       \
  X(words_peak, dictionary_words_peak)                         \
  X(words_bytes_peak, dictionary_bytes_peak)                   \
  X(words_created, dictionary_words_created)                   \
  X(words_collected, dictionary_words_collected)               \
  X(dictionary_rejects, dictionary_limit_rejected)             \
  X(tags_created, tag_descriptors_created)                     \
  X(tags_reused, tag_descriptor_lookups_reused)                \
  X(tags_collected, tag_descriptors_collected)                 \
  X(tag_gc_runs, tag_gc_runs)                                  \
  X(tag_gc_ns, tag_gc_ns)                                      \
  X(tag_gc_deferred, tag_gc_deferred)                          \
  X(tag_rejects, tag_limit_rejected)                           \
  X(staging_slots, staging_capacity_slots)                     \
  X(staging_bytes, staging_capacity_bytes)                     \
  X(queue_live, queued_datagrams_live)                         \
  X(queue_peak, queued_datagrams_peak)                         \
  X(pipeline_batches, pipeline_batches)                        \
  X(pipeline_ns, pipeline_wall_ns)                             \
  X(prepare_ns, request_prepare_ns)                            \
  X(words_write_wait_ns, dictionary_write_wait_ns)             \
  X(report_add_candidates, report_add_candidates)              \
  X(report_add_filtered, report_add_filtered)                  \
  X(report_delete_candidates, report_delete_candidates)        \
  X(report_delete_filtered, report_delete_filtered)            \
  X(report_update_ns, report_update_ns)                        \
  X(report_lock_wait_ns, report_lock_wait_ns)                  \
  X(gc_runs, dictionary_gc_runs)                               \
  X(gc_ns, dictionary_gc_ns)                                   \
  X(started_unix_us, started_unix_us)                          \
  X(started_steady_ns, started_steady_ns)                      \
  X(uptime_seconds, uptime_seconds)                            \
  X(raw_live, requests_live)                                   \
  X(timer_live, timers_live)                                   \
  X(raw_pool_bytes, request_pool_bytes)                        \
  X(timer_pool_bytes, timer_pool_bytes)                        \
  X(tags_live, tag_descriptors_live)                           \
  X(tags_bytes, tag_descriptor_bytes_known)                    \
  X(reports, report_count)                                     \
  X(report_rows, report_rows)                                  \
  X(report_struct_bytes, report_struct_bytes)                  \
  X(report_cpu_us, report_cpu_us)                              \
  X(workers, workers)                                          \
  X(healthy, healthy)                                          \
  X(receive_buffer_bytes, udp_receive_buffer_bytes)            \
  X(kernel_tracking, udp_kernel_drop_tracking)

enum class pinba_metric {
#define PINBA_METRIC_ENUM(id, name) id,
  PINBA_RUNTIME_METRICS(PINBA_METRIC_ENUM)
#undef PINBA_METRIC_ENUM
      count
};

inline std::array<std::atomic<uint64_t>, static_cast<size_t>(pinba_metric::count)> pinba_counters{};

inline auto &pinba_counter(pinba_metric metric) {
  return pinba_counters[static_cast<size_t>(metric)];
}
inline void pinba_count(pinba_metric metric, uint64_t count = 1) {
  pinba_counter(metric).fetch_add(count, std::memory_order_relaxed);
}
inline uint64_t pinba_count_get(pinba_metric metric) {
  return pinba_counter(metric).load(std::memory_order_relaxed);
}
inline void pinba_peak(pinba_metric metric, uint64_t value) {
  auto &counter = pinba_counter(metric);
  auto previous = counter.load(std::memory_order_relaxed);
  while (previous < value &&
         !counter.compare_exchange_weak(previous, value, std::memory_order_relaxed)) {
  }
}
inline uint64_t pinba_now_ns() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}
inline void pinba_runtime_reset() {
  for (auto &counter : pinba_counters) counter.store(0, std::memory_order_relaxed);
  pinba_counter(pinba_metric::started_steady_ns).store(pinba_now_ns());
  pinba_counter(pinba_metric::started_unix_us)
      .store(std::chrono::duration_cast<std::chrono::microseconds>(
                 std::chrono::system_clock::now().time_since_epoch())
                 .count());
}
struct pinba_metric_duration {
  pinba_metric metric;
  uint64_t started = pinba_now_ns();
  ~pinba_metric_duration() { pinba_count(metric, pinba_now_ns() - started); }
};

uint64_t pinba_metric_value(pinba_metric metric);

#endif
