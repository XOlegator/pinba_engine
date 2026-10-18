#include "pinba_runtime.h"

#include "pinba.h"
#include "pinba_map.h"

uint64_t pinba_metric_value(pinba_metric metric) {
  if (metric == pinba_metric::uptime_seconds) {
    return (pinba_now_ns() - pinba_count_get(pinba_metric::started_steady_ns)) / 1000000000;
  }
  if (!D) return pinba_count_get(metric);
  switch (metric) {
    case pinba_metric::healthy:
      return !D->in_shutdown.load();
    case pinba_metric::workers:
      return D->workers_ready.load();
    case pinba_metric::receive_buffer_bytes: {
      int value = 0;
      socklen_t size = sizeof(value);
      if (D->collector_socket)
        getsockopt(D->collector_socket->listen_sock, SOL_SOCKET, SO_RCVBUF, &value, &size);
      return value;
    }
    case pinba_metric::raw_live:
    case pinba_metric::raw_pool_bytes: {
      pthread_rwlock_rdlock(&D->collector_lock);
      const uint64_t value = metric == pinba_metric::raw_live
                                 ? pinba_pool_num_records(&D->request_pool)
                                 : D->request_pool.size * D->request_pool.element_size;
      pthread_rwlock_unlock(&D->collector_lock);
      return value;
    }
    case pinba_metric::timer_live:
    case pinba_metric::timer_pool_bytes: {
      pthread_rwlock_rdlock(&D->timer_lock);
      const uint64_t value = metric == pinba_metric::timer_live
                                 ? pinba_pool_num_records(&D->timer_pool)
                                 : D->timer_pool.size * D->timer_pool.element_size;
      pthread_rwlock_unlock(&D->timer_lock);
      return value;
    }
    case pinba_metric::tags_live:
    case pinba_metric::tags_bytes: {
      pthread_rwlock_rdlock(&D->words_lock);
      const uint64_t count = pinba_map_count(D->tag.name_index);
      pthread_rwlock_unlock(&D->words_lock);
      return metric == pinba_metric::tags_live ? count : count * sizeof(pinba_tag);
    }
    case pinba_metric::reports:
    case pinba_metric::report_rows:
    case pinba_metric::report_struct_bytes:
    case pinba_metric::report_cpu_us: {
      uint64_t value = 0;
      // Take one registry at a time. The result is a live observation rather
      // than a transactional snapshot across independently updated reports.
      const auto add = [&](pthread_rwlock_t &lock, const pinba_array_t &reports, size_t bytes) {
        pthread_rwlock_rdlock(&lock);
        for (size_t i = 0; i < reports.size; i++) {
          auto *report = static_cast<pinba_std_report *>(reports.data[i]);
          pthread_rwlock_rdlock(&report->lock);
          switch (metric) {
            case pinba_metric::reports:
              value++;
              break;
            case pinba_metric::report_rows:
              value += report->results_cnt;
              break;
            case pinba_metric::report_struct_bytes:
              value += bytes;
              break;
            default:
              value += static_cast<uint64_t>(report->ru_utime.tv_sec + report->ru_stime.tv_sec) *
                           1000000 +
                       report->ru_utime.tv_usec + report->ru_stime.tv_usec;
          }
          pthread_rwlock_unlock(&report->lock);
        }
        pthread_rwlock_unlock(&lock);
      };
      add(D->base_reports_lock, D->base_reports_arr, sizeof(pinba_report));
      add(D->rtag_reports_lock, D->rtag_reports_arr, sizeof(pinba_rtag_report));
      add(D->tag_reports_lock, D->tag_reports_arr, sizeof(pinba_tag_report));
      return value;
    }
    default:
      return pinba_count_get(metric);
  }
}
