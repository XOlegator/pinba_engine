/* Copyright (c) 2007-2013 Antony Dovgal <tony@daylessday.org>

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

#ifndef PINBA_H
#define PINBA_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

extern "C" {
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <math.h>
#include <sys/time.h>
#include <pthread.h>
#include <sys/resource.h>
}

#include "xxhash.h"
#include "pinba.pb-c.h"
#include "pinba_config.h"
#include "threadpool.h"
#include "pinba_types.h"
#include "pinba_lmap.h"

#undef P_SUCCESS
#undef P_FAILURE
#define P_SUCCESS 0
#define P_FAILURE -1


#define P_ERROR        (1<<0L)
#define P_WARNING      (1<<1L)
#define P_NOTICE       (1<<2L)
#define P_DEBUG        (1<<3L)
#define P_DEBUG_DUMP   (1<<4L)

char *pinba_error_ex(int return_error, int type, const char *file, int line, const char *format, ...);

#ifdef PINBA_DEBUG
#define pinba_debug(...) pinba_error_ex(0, P_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#else
#define pinba_debug(...)
#endif

#define pinba_warning(...) pinba_error_ex(0, P_WARNING, __FILE__, __LINE__, __VA_ARGS__)
#define pinba_error(type, ...) pinba_error_ex(0, type, __FILE__, __LINE__, __VA_ARGS__)
#define pinba_error_get(type, ...) pinba_error_ex(1, type, __FILE__, __LINE__, __VA_ARGS__)
extern pinba_daemon *D;

#ifdef __GNUC__
#define LIKELY(x)       __builtin_expect((x),1)
#define UNLIKELY(x)     __builtin_expect((x),0)
#else
#define LIKELY(x)       x
#define UNLIKELY(x)     x
#endif

#include <algorithm>
#include <cstddef>

void *pinba_data_main(void *arg);
void *pinba_collector_main(void *arg);
void *pinba_stats_main(void *arg);
int pinba_collector_init(const pinba_daemon_settings &settings);
void pinba_collector_shutdown();
int pinba_get_processors_number();

int pinba_get_time_interval();
int pinba_process_stats_packet(const unsigned char *buffer, int buffer_len);

void pinba_eat_udp(pinba_socket *socket, size_t thread_num);
void pinba_socket_free(pinba_socket *socket);
pinba_socket *pinba_socket_open(char *ip, int listen_port);

void pinba_tag_dtor(pinba_tag *tag);
int pinba_tag_put(const unsigned char *name);
pinba_tag *pinba_tag_get_by_name(char *name);
pinba_tag *pinba_tag_get_by_id(size_t id);
pinba_tag *pinba_tag_lookup_or_create_locked(const char *name, size_t len);

typedef struct _pinba_tag_metrics_snapshot {
	uint64_t tags_created;
	uint64_t tags_reused;
} pinba_tag_metrics_snapshot;

pinba_tag_metrics_snapshot pinba_tag_metrics_get(void);

#include "pinba_update_report_proto.h"

void pinba_update_add(pinba_array_t *array, size_t request_id, const pinba_stats_record *record);
void pinba_update_delete(pinba_array_t *array, size_t request_id, const pinba_stats_record *record);
void pinba_reports_destroy(void);
void pinba_tag_reports_destroy(void);
void pinba_rtag_reports_destroy(void);
void pinba_std_report_dtor(void *rprt);
void pinba_report_dtor(pinba_report *report, int lock_reports);
void pinba_tag_report_dtor(pinba_tag_report *report, int lock_tag_reports);
void pinba_rtag_report_dtor(pinba_rtag_report *report, int lock);

void pinba_update_tag_info_add(size_t request_id, void *report, const pinba_stats_record *record);
void pinba_update_tag_info_delete(size_t request_id, void *rep, const pinba_stats_record *record);
void pinba_update_tag2_info_add(size_t request_id, void *rep, const pinba_stats_record *record);
void pinba_update_tag2_info_delete(size_t request_id, void *rep, const pinba_stats_record *record);
void pinba_update_tag_report_add(size_t request_id, void *rep, const pinba_stats_record *record);
void pinba_update_tag_report_delete(size_t request_id, void *rep, const pinba_stats_record *record);
void pinba_update_tag2_report_add(size_t request_id, void *rep, const pinba_stats_record *record);
void pinba_update_tag2_report_delete(size_t request_id, void *rep, const pinba_stats_record *record);
void pinba_update_tag_report2_add(size_t request_id, void *rep, const pinba_stats_record *record);
void pinba_update_tag_report2_delete(size_t request_id, void *rep, const pinba_stats_record *record);
void pinba_update_tag2_report2_add(size_t request_id, void *rep, const pinba_stats_record *record);
void pinba_update_tag2_report2_delete(size_t request_id, void *rep, const pinba_stats_record *record);
void pinba_update_tagN_info_add(size_t request_id, void *rep, const pinba_stats_record *record);
void pinba_update_tagN_info_delete(size_t request_id, void *rep, const pinba_stats_record *record);
void pinba_update_tagN_report_add(size_t request_id, void *rep, const pinba_stats_record *record);
void pinba_update_tagN_report_delete(size_t request_id, void *rep, const pinba_stats_record *record);
void pinba_update_tagN_report2_add(size_t request_id, void *rep, const pinba_stats_record *record);
void pinba_update_tagN_report2_delete(size_t request_id, void *rep, const pinba_stats_record *record);

void pinba_update_rtag_info_add(size_t request_id, void *rep, const pinba_stats_record *record);
void pinba_update_rtag_info_delete(size_t request_id, void *rep, const pinba_stats_record *record);
void pinba_update_rtag2_info_add(size_t request_id, void *rep, const pinba_stats_record *record);
void pinba_update_rtag2_info_delete(size_t request_id, void *rep, const pinba_stats_record *record);
void pinba_update_rtagN_info_add(size_t request_id, void *rep, const pinba_stats_record *record);
void pinba_update_rtagN_info_delete(size_t request_id, void *rep, const pinba_stats_record *record);

void pinba_update_rtag_report_add(size_t request_id, void *rep, const pinba_stats_record *record);
void pinba_update_rtag_report_delete(size_t request_id, void *rep, const pinba_stats_record *record);
void pinba_update_rtag2_report_add(size_t request_id, void *rep, const pinba_stats_record *record);
void pinba_update_rtag2_report_delete(size_t request_id, void *rep, const pinba_stats_record *record);
void pinba_update_rtagN_report_add(size_t request_id, void *rep, const pinba_stats_record *record);
void pinba_update_rtagN_report_delete(size_t request_id, void *rep, const pinba_stats_record *record);

int pinba_array_add(pinba_array_t *array, void *tag_report);
int pinba_array_delete(pinba_array_t *array, void *tag_report);

int pinba_update_report_tables(pinba_std_report *std, char *index);
int pinba_delete_table_from_report_tables(char *index, char *name);
int pinba_delete_report_tables(char *index);

/* go over all new records in the pool */
#define pool_traverse_forward(i, pool) \
		for (i = (pool)->out; i != (pool)->in; i = (i == (pool)->size - 1) ? 0 : i + 1)

/* go over all records in the pool */
#define pool_traverse_backward(i, pool) \
			for (i = ((pool)->in > 0) ? (pool)->in - 1 : 0; \
                 i != ((pool)->out ? (pool)->out : ((pool)->in ? ((pool)->size - 1) : 0)); \
                 i = (i == 0) ? ((pool)->size - 1) : i - 1)

#define TMP_POOL(pool) ((pinba_tmp_stats_record *)((pool)->data))
#define REQ_DATA_POOL(pool) ((Pinba__Request **)((pool)->data))
#define REQ_POOL(pool) ((pinba_stats_record *)((pool)->data))
#define REQ_POOL_EX(pool) ((pinba_stats_record_ex *)((pool)->data))
#define TIMER_POOL(pool) ((pinba_timer_record *)((pool)->data))
#define POOL_DATA(pool) ((void **)((pool)->data))

static inline size_t pinba_copy_truncated(char *buf, size_t buf_size, const char *str, size_t str_len)
{
	if (!buf || buf_size == 0) {
		return 0;
	}

	const size_t copy_len = std::min(str_len, buf_size - 1);
	if (copy_len > 0 && str) {
		memcpy(buf, str, copy_len);
	}
	buf[copy_len] = '\0';
	return copy_len;
}

static inline void *pinba_malloc_or_log(size_t size, const char *what)
{
	void *ptr = malloc(size);
	if (UNLIKELY(!ptr)) {
		pinba_error(P_WARNING, "out of memory allocating %s (%zu bytes)", what, size);
	}
	return ptr;
}

static inline void *pinba_calloc_or_log(size_t count, size_t size, const char *what)
{
	void *ptr = calloc(count, size);
	if (UNLIKELY(!ptr)) {
		pinba_error(P_WARNING, "out of memory allocating %s (%zu x %zu bytes)", what, count, size);
	}
	return ptr;
}

static inline void *pinba_realloc_or_log(void *ptr, size_t size, const char *what)
{
	void *new_ptr = realloc(ptr, size);
	if (UNLIKELY(!new_ptr)) {
		pinba_error(P_WARNING, "out of memory reallocating %s (%zu bytes)", what, size);
	}
	return new_ptr;
}

static inline void *pinba_realloc_array_or_log(void *ptr, size_t count, size_t elem_size, const char *what)
{
	return pinba_realloc_or_log(ptr, count * elem_size, what);
}

static inline int pinba_ensure_reports_job_capacity(
	struct reports_job_data **buffer,
	unsigned int *allocated,
	size_t required,
	const char *buffer_name)
{
	struct reports_job_data *new_buffer;
	size_t new_allocated;

	if (*allocated >= required) {
		return P_SUCCESS;
	}

	new_allocated = required * 2;
	new_buffer = (struct reports_job_data *)pinba_realloc_array_or_log(
		*buffer, new_allocated, sizeof(struct reports_job_data), buffer_name);
	if (UNLIKELY(!new_buffer)) {
		return P_FAILURE;
	}

	*buffer = new_buffer;
	*allocated = new_allocated;
	return P_SUCCESS;
}

static inline size_t pinba_append_truncated(char *buf, size_t buf_size, size_t plus, const char *str, size_t str_len)
{
	if (!buf || buf_size == 0 || plus >= buf_size) {
		return plus;
	}

	const size_t available = buf_size - plus;
	if (available <= 1) {
		buf[buf_size - 1] = '\0';
		return buf_size - 1;
	}

	const size_t copy_len = std::min(str_len, available - 1);
	if (copy_len > 0 && str) {
		memcpy(buf + plus, str, copy_len);
	}
	buf[plus + copy_len] = '\0';
	return plus + copy_len;
}

#define memcpy_static(buf, str, str_len, result_len) \
	do { result_len = pinba_copy_truncated(buf, sizeof(buf), str, str_len); } while (0)

#define memcat_static(buf, plus, str, str_len, result_len) \
	do { result_len = pinba_append_truncated(buf, sizeof(buf), plus, str, str_len); } while (0)

size_t pinba_pool_num_records(pinba_pool *p);
int pinba_pool_init(pinba_pool *p, size_t size, size_t element_size, size_t limit_size, size_t grow_size, pool_dtor_func_t dtor, char *pool_name);
int pinba_pool_grow(pinba_pool *p, size_t more);
void pinba_pool_destroy(pinba_pool *p);
int pinba_pool_push(pinba_pool *p, size_t grow_size, void *data);

/* utility macros */

#define timeval_to_float(tv) ((float)(tv).tv_sec + ((float)(tv).tv_usec / 1000000.0))

#define timeval_to_pinba_timeval(tv, ptv) { ptv.tv_sec = tv.tv_sec; ptv.tv_usec = tv.tv_usec; }

#define pinba_timercmp(a, b, CMP) \
  (((a)->tv_sec == (b)->tv_sec) ? ((a)->tv_usec CMP (b)->tv_usec) : ((a)->tv_sec CMP (b)->tv_sec))

# define pinba_timeradd(a, b, result)                 \
  do {                                                \
    (result)->tv_sec = (a)->tv_sec + (b)->tv_sec;     \
    (result)->tv_usec = (a)->tv_usec + (b)->tv_usec;  \
    if ((result)->tv_usec >= 1000000)                 \
    {                                                 \
      ++(result)->tv_sec;                             \
      (result)->tv_usec -= 1000000;                   \
    }                                                 \
  } while (0)

# define pinba_timersub(a, b, result)                 \
  do {                                                \
    (result)->tv_sec = (a)->tv_sec - (b)->tv_sec;     \
    (result)->tv_usec = (a)->tv_usec - (b)->tv_usec;  \
    if ((result)->tv_usec < 0) {                      \
      --(result)->tv_sec;                             \
      (result)->tv_usec += 1000000;                   \
    }                                                 \
  } while (0)

static inline pinba_timeval float_to_timeval(double f) /* {{{ */
{
	pinba_timeval t;
	double fraction, integral;

	fraction = modf(f, &integral);
	t.tv_sec = (int)integral;
	t.tv_usec = (int)(fraction*1000000);
	return t;
}
/* }}} */

#define pinba_pool_is_full(pool) ((pool->in < pool->out) ? pool->size - (pool->out - pool->in) : (pool->in - pool->out)) == (pool->size - 1)

#define record_get_timer(pool, record, i) (((record->timers_start + i) >= (pool)->size) ? (TIMER_POOL((pool)) + (record->timers_start + i - (pool)->size)) : (TIMER_POOL((pool)) + (record->timers_start + i)))
#define record_get_timer_id(pool, record, i) ((record->timers_start + i) >= (pool)->size) ? ((record->timers_start + i) - (pool)->size) : ((record->timers_start + i))

#define CHECK_REPORT_CONDITIONS_CONTINUE(report, record)																\
	if (report->flags & PINBA_REPORT_CONDITIONAL) {																		\
		if (report->cond.min_time > 0.0 && timeval_to_float(record->data.req_time) < report->cond.min_time) {			\
			continue;																									\
		}																												\
		if (report->cond.max_time > 0.0 && timeval_to_float(record->data.req_time) > report->cond.max_time) {			\
			continue;																									\
		}																												\
	}																													\
	if (report->flags & PINBA_REPORT_TAGGED) {																			\
		unsigned int t1, t2;																							\
		unsigned int found_tags = 0;																					\
																														\
		if (!record->data.tags_cnt) {																					\
			continue;																									\
		}																												\
																														\
		for (t1 = 0; t1 < report->cond.tags_cnt; t1++) {																\
			for (t2 = 0; t2 < record->data.tags_cnt; t2++) {															\
				if (report->cond.tag_names[t1] == record->data.tag_names[t2]) {											\
					if (report->cond.tag_values[t1] == record->data.tag_values[t2]) {									\
						found_tags++;																					\
					} else {																							\
						/* found wrong value for the tag, so there's no point to continue searching */					\
						goto skip;																						\
					}																									\
				}																										\
			}																											\
		}																												\
																														\
		skip:																											\
																														\
		if (found_tags != report->cond.tags_cnt) {																		\
			continue;																									\
		}																												\
	}

int pinba_timer_mutex_lock();
int pinba_timer_mutex_unlock();

void pinba_per_thread_request_pool_dtor(void *pool);
void pinba_per_thread_tmp_pool_dtor(void *pool);

void pinba_data_pool_dtor(void *pool);
void pinba_temp_pool_dtor(void *pool);
void pinba_request_pool_dtor(void *pool);
void pinba_timer_pool_dtor(void *pool);

int timer_pool_add(int timers_cnt);

void update_reports_func(void *job_data);

void pinba_get_rusage(struct rusage *data);
void pinba_report_add_rusage(void *report, struct rusage *start_rusage);
pinba_word *pinba_dictionary_word_get_or_insert_rdlock(char *str, int str_len);

static inline void pinba_update_histogram(pinba_std_report *report, void **histogram_data, const pinba_timeval *time, const int add) /* {{{ */
{
	unsigned int slot_num;
	float time_value = timeval_to_float(*time);
	size_t value;

	if (add > 1) {
		time_value = time_value / add;
	} else if (add < -1) {
		time_value = time_value / -(add);
	}

	if (time_value > report->histogram_max_time) {
		slot_num = D->settings.histogram_size - 1;
	} else {
		slot_num = time_value / report->histogram_segment;
		if (slot_num > D->settings.histogram_size - 1) {
			slot_num = 0;
		}
	}

	value = (size_t)pinba_lmap_get(*histogram_data, slot_num);
	value += add;
	if (value == 0) {
		pinba_lmap_delete(*histogram_data, slot_num);
	} else {
		*histogram_data = pinba_lmap_add(*histogram_data, slot_num, (void *)value);
	}
}
/* }}} */

#define PINBA_UPDATE_HISTOGRAM_ADD(report, data, value) pinba_update_histogram((pinba_std_report *)(report), &(data), &(value), 1);
#define PINBA_UPDATE_HISTOGRAM_DEL(report, data, value) pinba_update_histogram((pinba_std_report *)(report), &(data), &(value), -1);
#define PINBA_UPDATE_HISTOGRAM_ADD_EX(report, data, value, cnt) pinba_update_histogram((pinba_std_report *)(report), &(data), &(value), (cnt));
#define PINBA_UPDATE_HISTOGRAM_DEL_EX(report, data, value, cnt) pinba_update_histogram((pinba_std_report *)(report), &(data), &(value), -(cnt));

#define PINBA_REPORT_DELETE_CHECK(report, record) if (pinba_timercmp(&(report)->std.start, &(record)->time, >) || (pinba_timercmp(&(report)->std.start, &(record)->time, ==) && (report)->std.request_pool_start_id > (record)->counter)) { return; }

struct pinba_version_info {
	const char *vcs_date;
	const char *vcs_branch;
	const char *vcs_full_hash;
	const char *vcs_short_hash;
	const char *vcs_wc_modified;
};

#ifndef PINBA_ENGINE_HAVE_STRNDUP
char *pinba_strndup(const char *s, unsigned int length);
#define strndup pinba_strndup
#endif

#endif /* PINBA_H */

/*
 * vim600: sw=4 ts=4 fdm=marker
 */
