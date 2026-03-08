#include <benchmark/benchmark.h>
#include <random>
#include <vector>

// Test includes
#include "pinba.h"
#include "pinba_types.h"

// Benchmark pool operations
static void BM_PoolPush(benchmark::State &state) {
  pinba_pool pool;
  pinba_pool_init(&pool, state.range(0), sizeof(int), 0, 0, nullptr,
                  "test_pool");

  for (auto _ : state) {
    int *data = new int(42);
    pinba_pool_push(&pool, 0, data);
  }

  pinba_pool_destroy(&pool);
  state.SetItemsProcessed(state.iterations());
}

static void BM_PoolTraversal(benchmark::State &state) {
  pinba_pool pool;
  pinba_pool_init(&pool, state.range(0), sizeof(int), 0, 0, nullptr,
                  "test_pool");

  // Pre-populate pool
  for (int i = 0; i < state.range(0); ++i) {
    int *data = new int(i);
    pinba_pool_push(&pool, 0, data);
  }

  for (auto _ : state) {
    size_t count = 0;
    for (size_t i = pool.out; i != pool.in;
         i = (i == pool.size - 1) ? 0 : i + 1) {
      count++;
    }
    benchmark::DoNotOptimize(count);
  }

  pinba_pool_destroy(&pool);
  state.SetItemsProcessed(state.iterations() * state.range(0));
}

// Benchmark hash operations
static void BM_HashFunction(benchmark::State &state) {
  std::string test_string(state.range(0), 'a');

  for (auto _ : state) {
    // This would benchmark XXHash
    // uint64_t hash = XXH64(test_string.c_str(), test_string.length(), 0);
    // benchmark::DoNotOptimize(hash);
  }

  state.SetItemsProcessed(state.iterations());
  state.SetBytesProcessed(state.iterations() * state.range(0));
}

// Benchmark memory operations
static void BM_MemoryAllocation(benchmark::State &state) {
  for (auto _ : state) {
    void *ptr = malloc(state.range(0));
    benchmark::DoNotOptimize(ptr);
    free(ptr);
  }

  state.SetItemsProcessed(state.iterations());
  state.SetBytesProcessed(state.iterations() * state.range(0));
}

// Register benchmarks
BENCHMARK(BM_PoolPush)->Range(1000, 1000000);
BENCHMARK(BM_PoolTraversal)->Range(1000, 1000000);
BENCHMARK(BM_HashFunction)->Range(8, 8 << 10);
BENCHMARK(BM_MemoryAllocation)->Range(8, 8 << 10);

BENCHMARK_MAIN();
