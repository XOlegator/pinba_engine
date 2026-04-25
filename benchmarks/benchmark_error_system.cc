/* Copyright (c) 2025 Pinba Engine Team
 *
 * Benchmarks for error handling system
 */

#include "pinba_error_modern.h"
#include <benchmark/benchmark.h>
#include <exception>

using namespace pinba;

static void BM_ErrorCodeCreation(benchmark::State &state) {
  for (auto _ : state) {
    auto ec = make_error_code(PinbaErrorCode::INVALID_PACKET);
    benchmark::DoNotOptimize(ec);
  }
}
BENCHMARK(BM_ErrorCodeCreation);

static void BM_ExceptionCreation(benchmark::State &state) {
  for (auto _ : state) {
    PinbaException ex(PinbaErrorCode::INVALID_PACKET, "Test error");
    benchmark::DoNotOptimize(ex);
  }
}
BENCHMARK(BM_ExceptionCreation);

static void BM_ResultSuccess(benchmark::State &state) {
  for (auto _ : state) {
    auto result = Result<int>::success(42);
    benchmark::DoNotOptimize(result);
  }
}
BENCHMARK(BM_ResultSuccess);

static void BM_ResultError(benchmark::State &state) {
  for (auto _ : state) {
    auto result =
        Result<int>::error(PinbaErrorCode::INVALID_PACKET, "Error message");
    benchmark::DoNotOptimize(result);
  }
}
BENCHMARK(BM_ResultError);

static void BM_ResultValueAccess(benchmark::State &state) {
  auto result = Result<int>::success(42);
  for (auto _ : state) {
    int value = result.value();
    benchmark::DoNotOptimize(value);
  }
}
BENCHMARK(BM_ResultValueAccess);

static void BM_ExceptionThrowCatch(benchmark::State &state) {
  for (auto _ : state) {
    try {
      throw PinbaException(PinbaErrorCode::INVALID_PACKET, "Test");
    } catch (const PinbaException &e) {
      benchmark::DoNotOptimize(e.what());
    }
  }
}
BENCHMARK(BM_ExceptionThrowCatch);
