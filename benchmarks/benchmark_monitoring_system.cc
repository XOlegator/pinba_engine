/* Copyright (c) 2025 Pinba Engine Team
 *
 * Benchmarks for monitoring system
 */

#include "pinba_monitoring_modern.h"
#include <benchmark/benchmark.h>
#include <thread>
#include <vector>

using namespace pinba;

static void BM_CounterIncrement(benchmark::State &state) {
  auto &counter = MetricsRegistry::instance().counter("bench_counter");
  counter.reset();

  for (auto _ : state) {
    counter.increment();
  }

  benchmark::DoNotOptimize(counter.get());
}
BENCHMARK(BM_CounterIncrement);

static void BM_CounterIncrementBatch(benchmark::State &state) {
  auto &counter = MetricsRegistry::instance().counter("bench_counter_batch");
  counter.reset();

  for (auto _ : state) {
    counter.increment(state.range(0));
  }

  benchmark::DoNotOptimize(counter.get());
}
BENCHMARK(BM_CounterIncrementBatch)->Arg(10)->Arg(100)->Arg(1000);

static void BM_GaugeSet(benchmark::State &state) {
  auto &gauge = MetricsRegistry::instance().gauge("bench_gauge");

  for (auto _ : state) {
    gauge.set(42);
  }

  benchmark::DoNotOptimize(gauge.get());
}
BENCHMARK(BM_GaugeSet);

static void BM_GaugeIncrement(benchmark::State &state) {
  auto &gauge = MetricsRegistry::instance().gauge("bench_gauge_inc");

  for (auto _ : state) {
    gauge.increment();
  }

  benchmark::DoNotOptimize(gauge.get());
}
BENCHMARK(BM_GaugeIncrement);

static void BM_HistogramObserve(benchmark::State &state) {
  std::vector<double> buckets = {0.1, 0.5, 1.0, 5.0, 10.0};
  auto &histogram =
      MetricsRegistry::instance().histogram("bench_histogram", buckets);

  for (auto _ : state) {
    histogram.observe(1.5);
  }

  benchmark::DoNotOptimize(histogram.to_prometheus("test"));
}
BENCHMARK(BM_HistogramObserve);

static void BM_PrometheusExport(benchmark::State &state) {
  auto &counter = MetricsRegistry::instance().counter("export_counter");
  auto &gauge = MetricsRegistry::instance().gauge("export_gauge");

  counter.increment(100);
  gauge.set(42);

  for (auto _ : state) {
    std::string prometheus = MetricsRegistry::instance().to_prometheus();
    benchmark::DoNotOptimize(prometheus);
  }
}
BENCHMARK(BM_PrometheusExport);

static void BM_HealthCheck(benchmark::State &state) {
  class TestCheck : public HealthCheck {
    HealthStatus check() const override { return HealthStatus::HEALTHY; }
    std::string message() const override { return "OK"; }
  };

  auto &registry = HealthRegistry::instance();
  registry.register_check("test", std::make_unique<TestCheck>());

  for (auto _ : state) {
    HealthStatus status = registry.overall_status();
    benchmark::DoNotOptimize(status);
  }
}
BENCHMARK(BM_HealthCheck);

static void BM_MetricRegistryAccess(benchmark::State &state) {
  for (auto _ : state) {
    auto &counter = MetricsRegistry::instance().counter("access_test");
    benchmark::DoNotOptimize(counter);
  }
}
BENCHMARK(BM_MetricRegistryAccess);
