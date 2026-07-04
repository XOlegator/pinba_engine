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

/*
 * Unit tests for the worker thread pool (src/threadpool.cc). The pool has no
 * dependency on the MySQL server, so its lifecycle, job dispatch, and barrier
 * synchronisation can be driven directly.
 *
 * Barrier lifecycle mirrors the engine's real usage (src/main.cc):
 *   init once -> { start; dispatch...; wait } repeated -> destroy once.
 * Barriers are malloc-allocated by the caller (init uses placement-new).
 */

#include <gtest/gtest.h>

#include <atomic>
#include <cstdlib>

#include "threadpool.h"

namespace {

/* RAII helper for a malloc-allocated barrier following the engine convention. */
struct ScopedBarrier {
  thread_pool_barrier_t *b;
  ScopedBarrier() : b(static_cast<thread_pool_barrier_t *>(malloc(sizeof(thread_pool_barrier_t)))) {
    th_pool_barrier_init(b);
  }
  ~ScopedBarrier() {
    th_pool_barrier_destroy(b);
    free(b);
  }
};

/* Job that increments a shared counter. */
void increment_job(void *arg) {
  auto *counter = static_cast<std::atomic<int> *>(arg);
  counter->fetch_add(1, std::memory_order_relaxed);
}

TEST(ThreadPool, CreateAndDestroyEmpty) {
  thread_pool_t *pool = th_pool_create(4);
  ASSERT_NE(pool, nullptr);
  EXPECT_EQ(pool->size, 4u);
  /* No jobs dispatched: destroy must still join all workers cleanly. */
  th_pool_destroy(pool);
}

TEST(ThreadPool, CreateInvalidThreadCountFails) {
  /* A pool needs at least one worker and no more than MAXT_IN_POOL. */
  EXPECT_EQ(th_pool_create(0), nullptr);
  EXPECT_EQ(th_pool_create(-1), nullptr);
  EXPECT_EQ(th_pool_create(MAXT_IN_POOL + 1), nullptr);
}

TEST(ThreadPool, CreateAtMaxThreads) {
  thread_pool_t *pool = th_pool_create(MAXT_IN_POOL);
  ASSERT_NE(pool, nullptr);
  EXPECT_EQ(pool->size, static_cast<size_t>(MAXT_IN_POOL));
  th_pool_destroy(pool);
}

TEST(ThreadPool, RunsDispatchedJobs) {
  thread_pool_t *pool = th_pool_create(4);
  ASSERT_NE(pool, nullptr);

  std::atomic<int> counter{0};
  ScopedBarrier barrier;

  constexpr int kJobs = 64;
  th_pool_barrier_start(barrier.b);
  for (int i = 0; i < kJobs; i++) {
    th_pool_dispatch(pool, barrier.b, increment_job, &counter);
  }
  th_pool_barrier_wait(barrier.b);

  EXPECT_EQ(counter.load(), kJobs);

  th_pool_destroy(pool);
}

/* Cleanup handler that flags it ran. */
void cleanup_flag(void *arg) {
  auto *flag = static_cast<std::atomic<bool> *>(arg);
  flag->store(true, std::memory_order_relaxed);
}

TEST(ThreadPool, RunsCleanupFunction) {
  thread_pool_t *pool = th_pool_create(2);
  ASSERT_NE(pool, nullptr);

  std::atomic<int> counter{0};
  std::atomic<bool> cleaned{false};
  ScopedBarrier barrier;

  th_pool_barrier_start(barrier.b);
  th_pool_dispatch_with_cleanup(pool, barrier.b, increment_job, &counter, cleanup_flag, &cleaned);
  th_pool_barrier_wait(barrier.b);

  EXPECT_EQ(counter.load(), 1);
  EXPECT_TRUE(cleaned.load());

  th_pool_destroy(pool);
}

TEST(ThreadPool, BarrierReusableAcrossBatches) {
  thread_pool_t *pool = th_pool_create(4);
  ASSERT_NE(pool, nullptr);

  std::atomic<int> counter{0};
  ScopedBarrier barrier;

  /* One init/destroy, several start/wait cycles — the engine's hot-loop pattern. */
  for (int batch = 0; batch < 3; batch++) {
    th_pool_barrier_start(barrier.b);
    for (int i = 0; i < 10; i++) {
      th_pool_dispatch(pool, barrier.b, increment_job, &counter);
    }
    th_pool_barrier_wait(barrier.b);
  }

  EXPECT_EQ(counter.load(), 30);

  th_pool_destroy(pool);
}

TEST(ThreadPool, MoreJobsThanInitialCapacity) {
  /* Dispatching far more jobs than the initial queue capacity forces the queue
   * to grow / block-and-recycle nodes; all jobs must still run exactly once. */
  thread_pool_t *pool = th_pool_create(2);
  ASSERT_NE(pool, nullptr);

  std::atomic<int> counter{0};
  ScopedBarrier barrier;

  constexpr int kJobs = 1000;
  th_pool_barrier_start(barrier.b);
  for (int i = 0; i < kJobs; i++) {
    th_pool_dispatch(pool, barrier.b, increment_job, &counter);
  }
  th_pool_barrier_wait(barrier.b);

  EXPECT_EQ(counter.load(), kJobs);

  th_pool_destroy(pool);
}

TEST(ThreadPool, BarrierEndWaitsAndDestroys) {
  /* th_pool_barrier_end() combines wait + destroy in one call. Exercise it on a
   * barrier we then only free (init already ran via placement-new). */
  thread_pool_t *pool = th_pool_create(2);
  ASSERT_NE(pool, nullptr);

  std::atomic<int> counter{0};
  auto *b = static_cast<thread_pool_barrier_t *>(malloc(sizeof(thread_pool_barrier_t)));
  ASSERT_NE(b, nullptr);
  th_pool_barrier_init(b);

  th_pool_barrier_start(b);
  for (int i = 0; i < 8; i++) {
    th_pool_dispatch(pool, b, increment_job, &counter);
  }
  th_pool_barrier_end(b);  // waits for completion, then destroys the barrier
  free(b);

  EXPECT_EQ(counter.load(), 8);

  th_pool_destroy_immediately(pool);
}

}  // namespace
