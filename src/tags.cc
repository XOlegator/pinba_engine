/* Copyright (c) 2007-2009 Antony Dovgal <tony@daylessday.org>
 * Copyright (c) 2025 Oleg Ekhlakov <subspam@mail.ru>
 *    Modified for MySQL 8.0/8.4 compatibility.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <algorithm>
#include <atomic>
#include <climits>
#include <new>
#include <vector>

#include "pinba.h"
#include "pinba_lmap.h"
#include "pinba_map.h"
#include "pinba_runtime.h"

void pinba_tag_dtor(pinba_tag *tag) /* {{{ */
{
  pinba_lmap_delete(D->tag.table, tag->id);
  pinba_map_delete(D->tag.name_index, tag->name);

  D->tag.live_ids.erase(tag->id);
  delete tag;
}
/* }}} */

pinba_tag *pinba_tag_get_by_name(char *name) /* {{{ */
{
  if (!name) return nullptr;
  pthread_rwlock_rdlock(&D->words_lock);
  auto *tag = static_cast<pinba_tag *>(pinba_map_get(D->tag.name_index, name));
  pthread_rwlock_unlock(&D->words_lock);
  return tag;
}

// Callers hold collector_lock while using the returned borrowed descriptor.
pinba_tag *pinba_tag_get_after(size_t id, bool exact) {
  pthread_rwlock_rdlock(&D->words_lock);
  pinba_tag *tag = nullptr;
  if (exact || !D->settings.tag_gc_enabled) {
    tag = static_cast<pinba_tag *>(pinba_lmap_get(D->tag.table, id));
  } else {
    auto it = D->tag.live_ids.lower_bound(id);
    if (it != D->tag.live_ids.end()) tag = it->second;
  }
  pthread_rwlock_unlock(&D->words_lock);
  return tag;
}

pinba_tag *pinba_tag_get_by_id(size_t id) { return pinba_tag_get_after(id, true); }

pinba_tag *pinba_tag_next_name(char *key, size_t capacity, bool first) {
  pthread_rwlock_rdlock(&D->words_lock);
  auto *tag = static_cast<pinba_tag *>(first ? pinba_map_first(D->tag.name_index, key, capacity)
                                             : pinba_map_next(D->tag.name_index, key, capacity));
  pthread_rwlock_unlock(&D->words_lock);
  return tag;
}

pinba_tag *pinba_tag_lookup_or_create_locked(const char *name, size_t len) /* {{{ */
{
  if (!name) {
    return nullptr;
  }

  pinba_tag *tag = (pinba_tag *)pinba_map_get(D->tag.name_index, (char *)name);
  if (tag) {
    pinba_count(pinba_metric::tags_reused);
    return tag;
  }

  /* upgrade lock */
  pthread_rwlock_unlock(&D->words_lock);
  const auto wait_started = pinba_now_ns();
  pthread_rwlock_wrlock(&D->words_lock);
  pinba_count(pinba_metric::words_write_wait_ns, pinba_now_ns() - wait_started);

  tag = (pinba_tag *)pinba_map_get(D->tag.name_index, (char *)name);
  if (!tag) {
    if ((D->settings.tag_names_limit &&
         pinba_lmap_count(D->tag.table) >= D->settings.tag_names_limit) ||
        D->tag.next_id > INT_MAX) {
      pinba_count(pinba_metric::tag_rejects);
      pthread_rwlock_unlock(&D->words_lock);
      pthread_rwlock_rdlock(&D->words_lock);
      return nullptr;
    }
    const size_t tag_id = D->tag.next_id;
    tag = new (std::nothrow) pinba_tag{};
    if (tag) {
      const size_t copied = pinba_copy_truncated(tag->name, sizeof(tag->name), name, len);
      tag->id = tag_id;
      tag->name_len = copied;
      tag->hash = 0;

      if (!pinba_map_try_add(&D->tag.name_index, tag->name, tag) ||
          !pinba_lmap_try_add(&D->tag.table, tag_id, tag)) {
        pinba_map_delete(D->tag.name_index, tag->name);
        delete tag;
        tag = nullptr;
      } else {
        bool indexed = true;
        if (D->settings.tag_gc_enabled) {
          try {
            D->tag.live_ids.emplace(tag_id, tag);
          } catch (const std::bad_alloc &) {
            indexed = false;
          }
        }
        if (indexed) {
          D->tag.next_id++;
          pinba_count(pinba_metric::tags_created);
        } else {
          pinba_lmap_delete(D->tag.table, tag_id);
          pinba_map_delete(D->tag.name_index, tag->name);
          delete tag;
          tag = nullptr;
        }
      }
    } else {
      pinba_warning("failed to allocate memory for a new tag");
    }
  } else {
    pinba_count(pinba_metric::tags_reused);
  }

  pthread_rwlock_unlock(&D->words_lock);
  pthread_rwlock_rdlock(&D->words_lock);
  return tag;
}
/* }}} */

pinba_tag_metrics_snapshot pinba_tag_metrics_get(void) /* {{{ */
{
  pinba_tag_metrics_snapshot snapshot;
  snapshot.tags_created = pinba_count_get(pinba_metric::tags_created);
  snapshot.tags_reused = pinba_count_get(pinba_metric::tags_reused);
  return snapshot;
}
/* }}} */

/*
 *
 * vim600: sw=4 ts=4 fdm=marker
 */

// Opt-in: historical tag catalogs remain persistent by default. collector_lock
// excludes report construction and SQL borrowed pointers; cursor pins span reads.
void pinba_collect_unused_tags() {
  if (!D->settings.tag_gc_enabled) return;
  const pinba_metric_duration duration{pinba_metric::tag_gc_ns};
  pinba_count(pinba_metric::tag_gc_runs);
  std::vector<size_t> selectors;
  pthread_rwlock_wrlock(&D->collector_lock);
  pthread_rwlock_rdlock(&D->tag_reports_lock);
  try {
    for (unsigned int i = 0; i < D->tag_reports_arr.size; ++i) {
      auto *report = static_cast<pinba_tag_report *>(D->tag_reports_arr.data[i]);
      for (int j = 0; j < report->tags_cnt; ++j) {
        if (report->tag_id[j] >= 0) selectors.push_back(report->tag_id[j]);
      }
    }
  } catch (const std::bad_alloc &) {
    pthread_rwlock_unlock(&D->tag_reports_lock);
    pthread_rwlock_unlock(&D->collector_lock);
    pinba_count(pinba_metric::tag_gc_deferred);
    return;
  }
  pthread_rwlock_unlock(&D->tag_reports_lock);
  std::sort(selectors.begin(), selectors.end());
  pthread_rwlock_wrlock(&D->words_lock);
  for (auto it = D->tag.live_ids.begin(); it != D->tag.live_ids.end();) {
    auto *tag = it->second;
    auto *word = static_cast<pinba_word *>(pinba_map_get(D->dictionary, tag->name));
    if (tag->cursor_references.load(std::memory_order_acquire) ||
        (word && word->references.load(std::memory_order_acquire)) ||
        std::binary_search(selectors.begin(), selectors.end(), tag->id)) {
      ++it;
      continue;
    }
    pinba_lmap_delete(D->tag.table, tag->id);
    pinba_map_delete(D->tag.name_index, tag->name);
    it = D->tag.live_ids.erase(it);
    delete tag;
    pinba_count(pinba_metric::tags_collected);
  }
  pthread_rwlock_unlock(&D->words_lock);
  pthread_rwlock_unlock(&D->collector_lock);
}
