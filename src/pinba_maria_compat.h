/* Copyright (c) 2026 Oleg Ekhlakov <o.ekhlakov@protonmail.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
*/

#ifndef PINBA_MARIA_COMPAT_H
#define PINBA_MARIA_COMPAT_H

/*
 * Source-level shims that let ha_pinba.cc build against both the MySQL 8.0/8.4
 * server API and the MariaDB 10.5+/11.x server API from a single source tree.
 *
 * Include this header AFTER the server headers (mysql_version.h, sql/handler.h,
 * sql/field.h, sql/table.h, my_bitmap.h) so that MARIADB_BASE_VERSION and the
 * referenced server types/constants are already visible. MariaDB defines
 * MARIADB_BASE_VERSION in its generated mysql_version.h; MySQL never does, so it
 * is the canonical compile-time switch between the two server dialects.
 *
 * The differences papered over here were enumerated by compiling ha_pinba.cc
 * against MariaDB 11.8 headers; each one is small and mechanical. The MySQL code
 * path is the unchanged upstream behaviour (the #else branches below).
 */

#ifdef MARIADB_BASE_VERSION

/* Field::field_index is a public data member in MariaDB, a getter method in
 * MySQL 8.x (which renamed the member to m_field_index). */
#define PINBA_FIELD_INDEX(f) ((f)->field_index)

/* MySQL 8.x exposes TABLE::set_no_row()/set_found_row(); MariaDB tracks the same
 * per-record state through TABLE::status (+ the null_row flag). */
#define PINBA_SET_NO_ROW(t)         \
  do {                              \
    (t)->status = STATUS_NOT_FOUND; \
    (t)->null_row = false;          \
  } while (0)
#define PINBA_SET_FOUND_ROW(t) \
  do {                         \
    (t)->status = 0;           \
    (t)->null_row = false;     \
  } while (0)

/* dbug_tmp_use_all_columns()/dbug_tmp_restore_column_map() take MY_BITMAP** on
 * MariaDB (the address of TABLE::write_set) and MY_BITMAP* on MySQL, and the
 * saved value is a MY_BITMAP* rather than a my_bitmap_map*. */
typedef MY_BITMAP *pinba_saved_column_map_t;
#define PINBA_TMP_USE_ALL_COLUMNS(table, bitmap) dbug_tmp_use_all_columns((table), &(bitmap))
#define PINBA_TMP_RESTORE_COLUMN_MAP(bitmap, saved) dbug_tmp_restore_column_map(&(bitmap), (saved))

/* MariaDB does not provide the SYS_VAR typedef used by MySQL 8.x for the plugin
 * system-variable table; the underlying struct is the same. */
#define SYS_VAR struct st_mysql_sys_var

#else /* MySQL 8.0 / 8.4 */

#define PINBA_FIELD_INDEX(f) ((f)->field_index())
#define PINBA_SET_NO_ROW(t) ((t)->set_no_row())
#define PINBA_SET_FOUND_ROW(t) ((t)->set_found_row())
typedef my_bitmap_map *pinba_saved_column_map_t;
#define PINBA_TMP_USE_ALL_COLUMNS(table, bitmap) dbug_tmp_use_all_columns((table), (bitmap))
#define PINBA_TMP_RESTORE_COLUMN_MAP(bitmap, saved) dbug_tmp_restore_column_map((bitmap), (saved))

#endif /* MARIADB_BASE_VERSION */

#endif /* PINBA_MARIA_COMPAT_H */
