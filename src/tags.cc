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

#include <atomic>

#include "pinba.h"
#include "pinba_lmap.h"
#include "pinba_map.h"
#include "pinba_runtime.h"

void pinba_tag_dtor(pinba_tag *tag) /* {{{ */
{
  pinba_lmap_delete(D->tag.table, tag->id);
  pinba_map_delete(D->tag.name_index, tag->name);

  free(tag);
}
/* }}} */

pinba_tag *pinba_tag_get_by_name(char *name) /* {{{ */
{
  pinba_tag *tag;

  tag = (pinba_tag *)pinba_map_get(D->tag.name_index, name);
  if (UNLIKELY(!name)) {
    return nullptr;
  }

  return tag;
}
/* }}} */

pinba_tag *pinba_tag_get_by_id(size_t id) /* {{{ */
{
  pinba_tag *tag;

  tag = (pinba_tag *)pinba_lmap_get(D->tag.table, id);
  if (UNLIKELY(!tag)) {
    return nullptr;
  }

  return tag;
}
/* }}} */

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
    const size_t tag_id = pinba_lmap_count(D->tag.table);
    tag = (pinba_tag *)malloc(sizeof(pinba_tag));
    if (tag) {
      const size_t copied = pinba_copy_truncated(tag->name, sizeof(tag->name), name, len);
      tag->id = tag_id;
      tag->name_len = copied;
      tag->hash = 0;

      if (!pinba_map_try_add(&D->tag.name_index, tag->name, tag) ||
          !pinba_lmap_try_add(&D->tag.table, tag_id, tag)) {
        pinba_map_delete(D->tag.name_index, tag->name);
        free(tag);
        tag = nullptr;
      } else {
        pinba_count(pinba_metric::tags_created);
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
