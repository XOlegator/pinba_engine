/* Copyright (c) 2025 Pinba Engine Team
 *
 * Benchmarks for logging system
 */

#include "pinba_logging_modern.h"
#include <benchmark/benchmark.h>
#include <filesystem>
#include <fstream>

using namespace pinba;

static void BM_LogEntryCreation(benchmark::State &state) {
  for (auto _ : state) {
    LogEntry entry(LogLevel::INFO, __FILE__, __LINE__, __FUNCTION__,
                   "Test message");
    benchmark::DoNotOptimize(entry);
  }
}
BENCHMARK(BM_LogEntryCreation);

static void BM_LogLevelToString(benchmark::State &state) {
  for (auto _ : state) {
    const char *str = log_level_to_string(LogLevel::INFO);
    benchmark::DoNotOptimize(str);
  }
}
BENCHMARK(BM_LogLevelToString);

static void BM_LoggerLogSync(benchmark::State &state) {
  Logger::instance().shutdown(); // Use sync mode

  for (auto _ : state) {
    PINBA_LOG_INFO("Benchmark message");
  }

  Logger::instance().flush();
}
BENCHMARK(BM_LoggerLogSync);

static void BM_LoggerLogAsync(benchmark::State &state) {
  std::string log_file =
      "/tmp/pinba_bench_" + std::to_string(getpid()) + ".log";
  Logger::instance().initialize(log_file, LogLevel::INFO);

  for (auto _ : state) {
    PINBA_LOG_INFO("Benchmark message");
  }

  Logger::instance().flush();
  Logger::instance().shutdown();
  std::remove(log_file.c_str());
}
BENCHMARK(BM_LoggerLogAsync);

static void BM_StructuredLogging(benchmark::State &state) {
  Logger::instance().shutdown();

  std::string fields = R"({"key":"value","number":42})";
  for (auto _ : state) {
    PINBA_LOG_WITH_FIELDS(LogLevel::INFO, "Message", fields);
  }

  Logger::instance().flush();
}
BENCHMARK(BM_StructuredLogging);

BENCHMARK_MAIN();
