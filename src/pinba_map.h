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

#ifndef HAVE_PINBA_MAP_H
#define HAVE_PINBA_MAP_H

#include <cstddef>

void *pinba_map_first(void *map_report, char *index_to_fill, size_t buf_size = 8192);
void *pinba_map_next(void *map_report, char *index_to_fill, size_t buf_size = 8192);
void *pinba_map_get(void *map_report, const char *index);
void *pinba_map_add(void *map_report, const char *index, const void *report);
int pinba_map_delete(void *map_report, const char *index);
void pinba_map_destroy(void *data);
void *pinba_map_create();
size_t pinba_map_count(void *map_report);

#endif /* HAVE_PINBA_MAP_H */
