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

#include "pinba_config.h"

#ifndef PINBA_LIMITS_H
#define PINBA_LIMITS_H

/* max index string length */
#define PINBA_MAX_LINE_LEN 8192

/* these must not be greater than 255! */
#define PINBA_HOSTNAME_SIZE 65
#define PINBA_SERVER_NAME_SIZE 65
#define PINBA_SCRIPT_NAME_SIZE 129
#define PINBA_STATUS_SIZE 33
#define PINBA_SCHEMA_SIZE 17

#define PINBA_TAG_NAME_SIZE 65
#define PINBA_TAG_VALUE_SIZE 65
#define PINBA_DICTIONARY_ENTRY_SIZE 65 /* must be equal to the greater of the two above */

#define PINBA_ERR_BUFFER 2048

#define PINBA_UDP_BUFFER_SIZE 65536

#define PINBA_DICTIONARY_GROW_SIZE 32
#define PINBA_TIMER_POOL_GROW_SIZE 2621440
#define PINBA_TIMER_POOL_SHRINK_SIZE PINBA_TIMER_POOL_GROW_SIZE * 5

#define PINBA_THREAD_POOL_DEFAULT_SIZE 8
#define PINBA_THREAD_POOL_THRESHOLD_AMOUNT 16
#define PINBA_MIN_TAG_VALUES_CNT_MAGIC_NUMBER 8
#define PINBA_PER_THREAD_POOL_GROW_SIZE 1024
#define PINBA_TEMP_DICTIONARY_SIZE 1024

#endif
