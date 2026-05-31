/* Copyright (c) 2025 Oleg Ekhlakov <subspam@mail.ru>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef HAVE_PINBA_LMAP_H
#define HAVE_PINBA_LMAP_H

void *pinba_lmap_first(void *map_report, uint64_t *index_to_fill);
void *pinba_lmap_next(void *map_report, uint64_t *index_to_fill);
void *pinba_lmap_get(void *map_report, uint64_t index);
void *pinba_lmap_add(void *map_report, uint64_t index, const void *report);
int pinba_lmap_delete(void *map_report, uint64_t index);
void pinba_lmap_destroy(void *data);
void *pinba_lmap_create();
size_t pinba_lmap_count(void *map_report);

#endif /* HAVE_PINBA_LMAP_H */
