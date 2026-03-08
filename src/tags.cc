/* Copyright (c) 2007-2009 Antony Dovgal <tony@daylessday.org>

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

#include "pinba.h"
#include "pinba_map.h"
#include "pinba_lmap.h"

#include <atomic>

static std::atomic<uint64_t> g_tags_created{0};
static std::atomic<uint64_t> g_tags_reused{0};

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
		return NULL;
	}

	return tag;
}
/* }}} */

pinba_tag *pinba_tag_get_by_id(size_t id) /* {{{ */
{
	pinba_tag *tag;

	tag = (pinba_tag *)pinba_lmap_get(D->tag.table, id);
	if (UNLIKELY(!tag)) {
		return NULL;
	}

	return tag;
}
/* }}} */

pinba_tag *pinba_tag_lookup_or_create_locked(const char *name, size_t len) /* {{{ */
{
	if (!name) {
		return NULL;
	}

	pinba_tag *tag = (pinba_tag *)pinba_map_get(D->tag.name_index, (char *)name);
	if (tag) {
		g_tags_reused.fetch_add(1, std::memory_order_relaxed);
		return tag;
	}

	/* upgrade lock */
	pthread_rwlock_unlock(&D->words_lock);
	pthread_rwlock_wrlock(&D->words_lock);

	tag = (pinba_tag *)pinba_map_get(D->tag.name_index, (char *)name);
	if (!tag) {
		const size_t tag_id = pinba_lmap_count(D->tag.table);
		tag = (pinba_tag *)malloc(sizeof(pinba_tag));
		if (tag) {
			const size_t copied = pinba_copy_truncated(tag->name, sizeof(tag->name), name, len);
			tag->id = tag_id;
			tag->name_len = copied;
			tag->hash = 0;

			D->tag.table = pinba_lmap_add(D->tag.table, tag_id, tag);
			D->tag.name_index = pinba_map_add(D->tag.name_index, tag->name, tag);
			g_tags_created.fetch_add(1, std::memory_order_relaxed);
		} else {
			pinba_warning("failed to allocate memory for a new tag");
		}
	} else {
		g_tags_reused.fetch_add(1, std::memory_order_relaxed);
	}

	pthread_rwlock_unlock(&D->words_lock);
	pthread_rwlock_rdlock(&D->words_lock);
	return tag;
}
/* }}} */

pinba_tag_metrics_snapshot pinba_tag_metrics_get(void) /* {{{ */
{
	pinba_tag_metrics_snapshot snapshot;
	snapshot.tags_created = g_tags_created.load(std::memory_order_relaxed);
	snapshot.tags_reused = g_tags_reused.load(std::memory_order_relaxed);
	return snapshot;
}
/* }}} */

/* 
 *
 * vim600: sw=4 ts=4 fdm=marker
 */
