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

#include "threadpool.h"

#include <cstdlib>
#include <new>

#include "pinba.h"

#define MAX_QUEUE_MEMORY_SIZE 65536

// ---------------------------------------------------------------------------
// Internal job queue (unchanged C-style linked list, protected by pool->mutex)
// ---------------------------------------------------------------------------

static inline queue_head_t *queue_create(int initial_cap) /* {{{ */
{
  queue_head_t *theQueue = static_cast<queue_head_t *>(malloc(sizeof(queue_head_t)));
  int max_cap = MAX_QUEUE_MEMORY_SIZE / static_cast<int>(sizeof(queue_node_t));

  if (theQueue == nullptr) {
    return nullptr;
  }

  if (initial_cap > max_cap) {
    initial_cap = max_cap;
  }

  if (initial_cap == 0) {
    free(theQueue);
    return nullptr;
  }

  theQueue->capacity = initial_cap;
  theQueue->posted = 0;
  theQueue->max_capacity = max_cap;
  theQueue->head = nullptr;
  theQueue->tail = nullptr;
  theQueue->freeHead = static_cast<queue_node_t *>(malloc(sizeof(queue_node_t)));

  if (theQueue->freeHead == nullptr) {
    free(theQueue);
    return nullptr;
  }
  theQueue->freeTail = theQueue->freeHead;

  for (int i = 0; i < initial_cap; i++) {
    queue_node_t *temp = static_cast<queue_node_t *>(malloc(sizeof(queue_node_t)));
    if (temp == nullptr) {
      return theQueue;
    }
    temp->next = theQueue->freeHead;
    temp->prev = nullptr;
    theQueue->freeHead->prev = temp;
    theQueue->freeHead = temp;
  }

  return theQueue;
}
/* }}} */

static inline void queue_destroy(queue_head_t *queue) /* {{{ */
{
  queue_node_t *temp = queue->head;
  while (temp) {
    queue_node_t *node = temp;
    temp = node->next;
    free(node);
  }

  temp = queue->freeHead;
  while (temp) {
    queue_node_t *node = temp;
    temp = node->next;
    free(node);
  }
  free(queue);
}
/* }}} */

static inline void queue_post_job(queue_head_t *theQueue, thread_pool_barrier_t *barrier,
                                  dispatch_fn_t func1, void *arg1, dispatch_fn_t func2,
                                  void *arg2) /* {{{ */
{
  queue_node_t *temp;

  if (theQueue->freeTail == nullptr) {
    temp = static_cast<queue_node_t *>(malloc(sizeof(queue_node_t)));
    if (temp == nullptr) {
      return;
    }
    temp->next = nullptr;
    temp->prev = nullptr;
    theQueue->freeHead = temp;
    theQueue->freeTail = temp;
    theQueue->capacity++;
  }

  temp = theQueue->freeTail;
  if (theQueue->freeTail->prev == nullptr) {
    theQueue->freeTail = nullptr;
    theQueue->freeHead = nullptr;
  } else {
    theQueue->freeTail->prev->next = nullptr;
    theQueue->freeTail = theQueue->freeTail->prev;
    theQueue->freeTail->next = nullptr;
  }

  theQueue->posted++;
  temp->func_to_dispatch = func1;
  temp->func_arg = arg1;
  temp->cleanup_func = func2;
  temp->cleanup_arg = arg2;
  temp->barrier = barrier;
  if (barrier) {
    barrier->posted_count++;
  }

  temp->prev = theQueue->tail;
  temp->next = nullptr;

  if (theQueue->tail) {
    theQueue->tail->next = temp;
  } else {
    theQueue->head = temp;
  }
  theQueue->tail = temp;
}
/* }}} */

static inline void queue_fetch_job(queue_head_t *theQueue, thread_pool_barrier_t **barrier,
                                   dispatch_fn_t *func1, void **arg1, dispatch_fn_t *func2,
                                   void **arg2) /* {{{ */
{
  queue_node_t *temp = theQueue->tail;
  if (temp == nullptr) {
    return;
  }

  if (temp->prev) {
    temp->prev->next = nullptr;
  } else {
    theQueue->head = nullptr;
  }
  theQueue->tail = temp->prev;

  *func1 = temp->func_to_dispatch;
  *arg1 = temp->func_arg;
  *func2 = temp->cleanup_func;
  *arg2 = temp->cleanup_arg;
  *barrier = temp->barrier;

  temp->next = nullptr;
  temp->prev = nullptr;
  if (theQueue->freeHead == nullptr) {
    theQueue->freeTail = temp;
    theQueue->freeHead = temp;
  } else {
    temp->next = theQueue->freeHead;
    theQueue->freeHead->prev = temp;
    theQueue->freeHead = temp;
  }
}
/* }}} */

static inline int queue_can_accept_order(queue_head_t *theQueue) /* {{{ */
{
  return (theQueue->freeTail != nullptr || theQueue->capacity <= theQueue->max_capacity);
}
/* }}} */

static inline int queue_is_job_available(queue_head_t *theQueue) /* {{{ */
{
  return (theQueue->tail != nullptr);
}
/* }}} */

// ---------------------------------------------------------------------------
// Worker thread
// ---------------------------------------------------------------------------

// std::unique_lock provides RAII mutex management, replacing pthread_cleanup_push.
static void th_do_work(thread_pool_t *pool) /* {{{ */
{
  std::unique_lock<std::mutex> lock(pool->mutex);

  for (;;) {
    pool->job_posted.wait(lock, [pool] {
      return pool->shutdown || queue_is_job_available(pool->job_queue);
    });

    // Drain remaining jobs before honouring shutdown.
    if (pool->shutdown && !queue_is_job_available(pool->job_queue)) {
      break;
    }

    dispatch_fn_t myjob = nullptr;
    dispatch_fn_t mycleaner = nullptr;
    void *myarg = nullptr;
    void *mycleanarg = nullptr;
    thread_pool_barrier_t *barrier = nullptr;

    queue_fetch_job(pool->job_queue, &barrier, &myjob, &myarg, &mycleaner, &mycleanarg);
    pool->job_taken.notify_one();
    lock.unlock();

    myjob(myarg);
    // Cleanup runs unconditionally (pthread cancel is gone; callers in this
    // codebase always pass nullptr for cleaner_func).
    if (mycleaner != nullptr) {
      mycleaner(mycleanarg);
    }
    if (barrier != nullptr) {
      th_pool_barrier_signal(barrier);
    }

    lock.lock();
  }

  --pool->live;
  pool->job_taken.notify_one();
}
/* }}} */

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

thread_pool_t *th_pool_create(int num_threads_in_pool) /* {{{ */
{
  if (num_threads_in_pool <= 0 || num_threads_in_pool > MAXT_IN_POOL) {
    return nullptr;
  }

  auto *pool = new (std::nothrow) thread_pool_t{};
  if (pool == nullptr) {
    return nullptr;
  }

  pool->shutdown = false;
  pool->size = static_cast<size_t>(num_threads_in_pool);
  pool->live = pool->size;
  pool->job_queue = queue_create(num_threads_in_pool);
  if (pool->job_queue == nullptr) {
    delete pool;
    return nullptr;
  }

  pool->threads.reserve(pool->size);
  for (size_t i = 0; i < pool->size; ++i) {
    pool->threads.emplace_back(th_do_work, pool);
  }

  return pool;
}
/* }}} */

void th_pool_dispatch_with_cleanup(thread_pool_t *from_me, thread_pool_barrier_t *barrier,
                                   dispatch_fn_t dispatch_to_here, void *arg,
                                   dispatch_fn_t cleaner_func,
                                   void *cleaner_arg) /* {{{ */
{
  std::unique_lock<std::mutex> lock(from_me->mutex);

  from_me->job_taken.wait(lock, [from_me] {
    return queue_can_accept_order(from_me->job_queue);
  });

  queue_post_job(from_me->job_queue, barrier, dispatch_to_here, arg, cleaner_func, cleaner_arg);
  from_me->job_posted.notify_one();
}
/* }}} */

void th_pool_destroy(thread_pool_t *pool) /* {{{ */
{
  {
    std::unique_lock<std::mutex> lock(pool->mutex);
    pool->shutdown = true;
  }
  pool->job_posted.notify_all();

  for (auto &t : pool->threads) {
    if (t.joinable()) {
      t.join();
    }
  }

  queue_destroy(pool->job_queue);
  delete pool;
}
/* }}} */

// std::thread cannot be cancelled mid-execution; drain and join instead.
void th_pool_destroy_immediately(thread_pool_t *pool) /* {{{ */
{
  th_pool_destroy(pool);
}
/* }}} */

// ---------------------------------------------------------------------------
// Barrier
//
// Barriers are malloc-allocated by callers.  th_pool_barrier_init uses
// placement new to construct the C++ members (std::mutex, std::condition_variable)
// in that malloc'd block; th_pool_barrier_destroy calls the destructor explicitly.
// ---------------------------------------------------------------------------

void th_pool_barrier_init(thread_pool_barrier_t *b) /* {{{ */
{
  new (b) thread_pool_barrier_t{};
  b->posted_count = 0;
  b->done_count = 0;
}
/* }}} */

int th_pool_barrier_start(thread_pool_barrier_t *b) /* {{{ */
{
  b->posted_count = 0;
  b->done_count = 0;
  return 0;
}
/* }}} */

void th_pool_barrier_signal(thread_pool_barrier_t *b) /* {{{ */
{
  {
    std::unique_lock<std::mutex> lock(b->mutex);
    ++b->done_count;
  }
  b->var.notify_one();
}
/* }}} */

void th_pool_barrier_wait(thread_pool_barrier_t *b) /* {{{ */
{
  std::unique_lock<std::mutex> lock(b->mutex);
  b->var.wait(lock, [b] { return b->done_count >= b->posted_count; });
}
/* }}} */

void th_pool_barrier_end(thread_pool_barrier_t *b) /* {{{ */
{
  th_pool_barrier_wait(b);
  th_pool_barrier_destroy(b);
}
/* }}} */

void th_pool_barrier_destroy(thread_pool_barrier_t *b) /* {{{ */
{
  b->~thread_pool_barrier_t();
}
/* }}} */
