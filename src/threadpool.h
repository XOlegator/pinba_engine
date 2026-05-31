/* Copyright (c) 2007-2009 Antony Dovgal <tony@daylessday.org>
 * Copyright (c) 2025 Oleg Ekhlakov <subspam@mail.ru>
 *    Rewritten from pthread to std::thread/std::mutex/std::condition_variable.
 *
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

#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <thread>
#include <vector>

// maximum number of threads allowed in a pool
#define MAXT_IN_POOL 200

typedef void (*dispatch_fn_t)(void *);

// Barrier: synchronises a batch of dispatched jobs.
// Must be allocated with malloc and initialised via th_pool_barrier_init()
// before use; th_pool_barrier_destroy() must be called before free().
typedef struct _thread_pool_barrier_t {
  std::mutex mutex;
  std::condition_variable var;
  int posted_count;
  int done_count;
} thread_pool_barrier_t;

typedef struct _queue_node_t queue_node_t;

struct _queue_node_t {
  dispatch_fn_t func_to_dispatch;
  void *func_arg;
  dispatch_fn_t cleanup_func;
  void *cleanup_arg;
  thread_pool_barrier_t *barrier;
  queue_node_t *next;
  queue_node_t *prev;
};

typedef struct _queue_head_t {
  queue_node_t *head;
  queue_node_t *tail;
  queue_node_t *freeHead;
  queue_node_t *freeTail;
  int capacity;
  int max_capacity;
  int posted;
} queue_head_t;

typedef struct _thread_pool_t {
  std::vector<std::thread> threads;  // worker threads (joinable, not detached)
  std::mutex mutex;                  // protects job_queue, shutdown, live
  std::condition_variable job_posted;
  std::condition_variable job_taken;
  bool shutdown;
  size_t size;  // total thread count
  size_t live;  // threads still running (decremented on exit)
  queue_head_t *job_queue;
} thread_pool_t;

// Creates a fixed-size thread pool. Returns NULL on failure.
thread_pool_t *th_pool_create(int num_threads_in_pool);

// Dispatches a job with an optional cleanup function.
// Blocks if the job queue is full until a slot becomes available.
void th_pool_dispatch_with_cleanup(thread_pool_t *from_me, thread_pool_barrier_t *barrier,
                                   dispatch_fn_t dispatch_to_here, void *arg,
                                   dispatch_fn_t cleaner_func, void *cleaner_arg);

#define th_pool_dispatch(from, barrier, to, arg) \
  th_pool_dispatch_with_cleanup((from), (barrier), (to), (arg), nullptr, nullptr)

// Signals all threads to finish remaining queued jobs, joins them, frees pool.
void th_pool_destroy(thread_pool_t *destroyme);

// Same as th_pool_destroy: std::thread cannot be cancelled, so remaining jobs
// are drained and threads are joined gracefully.
void th_pool_destroy_immediately(thread_pool_t *destroymenow);

// Barrier lifecycle: init (placement-new) → start/wait cycles → destroy (dtor).
void th_pool_barrier_init(thread_pool_barrier_t *b);
int th_pool_barrier_start(thread_pool_barrier_t *b);
void th_pool_barrier_signal(thread_pool_barrier_t *b);
void th_pool_barrier_wait(thread_pool_barrier_t *b);
void th_pool_barrier_end(thread_pool_barrier_t *b);
void th_pool_barrier_destroy(thread_pool_barrier_t *b);

#endif  // THREADPOOL_H
