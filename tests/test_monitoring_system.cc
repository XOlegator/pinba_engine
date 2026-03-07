/* Copyright (c) 2025 Pinba Engine Team
 *
 * Unit tests for modern monitoring system
 */

#include "pinba_monitoring_modern.h"
#include <gtest/gtest.h>
#include <thread>
#include <vector>

using namespace pinba;

class MonitoringSystemTest : public ::testing::Test {
protected:
  void SetUp() override {}

  void TearDown() override {}
};

// Test Counter metric
TEST_F(MonitoringSystemTest, CounterMetric) {
  auto &counter = MetricsRegistry::instance().counter("test_counter");

  EXPECT_EQ(counter.get(), 0);

  counter.increment();
  EXPECT_EQ(counter.get(), 1);

  counter.increment(5);
  EXPECT_EQ(counter.get(), 6);

  counter.reset();
  EXPECT_EQ(counter.get(), 0);
}

TEST_F(MonitoringSystemTest, CounterThreadSafety) {
  auto &counter = MetricsRegistry::instance().counter("thread_test_counter");

  const int num_threads = 10;
  const int increments_per_thread = 100;

  std::vector<std::thread> threads;
  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([&counter, increments_per_thread]() {
      for (int j = 0; j < increments_per_thread; ++j) {
        counter.increment();
      }
    });
  }

  for (auto &t : threads) {
    t.join();
  }

  EXPECT_EQ(counter.get(), num_threads * increments_per_thread);
}

// Test Gauge metric
TEST_F(MonitoringSystemTest, GaugeMetric) {
  auto &gauge = MetricsRegistry::instance().gauge("test_gauge");

  EXPECT_EQ(gauge.get(), 0);

  gauge.set(42);
  EXPECT_EQ(gauge.get(), 42);

  gauge.increment(10);
  EXPECT_EQ(gauge.get(), 52);

  gauge.decrement(5);
  EXPECT_EQ(gauge.get(), 47);
}

TEST_F(MonitoringSystemTest, GaugeThreadSafety) {
  auto &gauge = MetricsRegistry::instance().gauge("thread_test_gauge");

  const int num_threads = 10;

  std::vector<std::thread> threads;
  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([&gauge, i]() { gauge.set(i); });
  }

  for (auto &t : threads) {
    t.join();
  }

  // Final value should be one of the set values
  int final_value = gauge.get();
  EXPECT_GE(final_value, 0);
  EXPECT_LE(final_value, num_threads - 1);
}

// Test Histogram metric
TEST_F(MonitoringSystemTest, HistogramMetric) {
  std::vector<double> buckets = {0.1, 0.5, 1.0, 5.0, 10.0};
  auto &histogram =
      MetricsRegistry::instance().histogram("test_histogram", buckets);

  histogram.observe(0.05);
  histogram.observe(0.3);
  histogram.observe(0.7);
  histogram.observe(2.0);
  histogram.observe(7.0);
  histogram.observe(15.0);

  // Check that observations were recorded
  std::string prometheus = histogram.to_prometheus("test_histogram");
  EXPECT_NE(prometheus.find("test_histogram_count"), std::string::npos);
  EXPECT_NE(prometheus.find("test_histogram_sum"), std::string::npos);
}

// Test Prometheus export
TEST_F(MonitoringSystemTest, PrometheusExport) {
  auto &counter = MetricsRegistry::instance().counter("export_counter");
  auto &gauge = MetricsRegistry::instance().gauge("export_gauge");

  counter.increment(10);
  gauge.set(42);

  std::string prometheus = MetricsRegistry::instance().to_prometheus();

  EXPECT_NE(prometheus.find("pinba_export_counter"), std::string::npos);
  EXPECT_NE(prometheus.find("pinba_export_gauge"), std::string::npos);
  EXPECT_NE(prometheus.find("10"), std::string::npos);
  EXPECT_NE(prometheus.find("42"), std::string::npos);
  EXPECT_NE(prometheus.find("# TYPE"), std::string::npos);
}

// Test Health checks
TEST_F(MonitoringSystemTest, HealthCheck) {
  class TestHealthCheck : public HealthCheck {
  public:
    TestHealthCheck(HealthStatus status, const std::string &msg)
        : status_(status), message_(msg) {}

    HealthStatus check() const override { return status_; }

    std::string message() const override { return message_; }

  private:
    HealthStatus status_;
    std::string message_;
  };

  auto &registry = HealthRegistry::instance();

  registry.register_check(
      "test1", std::make_unique<TestHealthCheck>(HealthStatus::HEALTHY, "OK"));
  registry.register_check("test2", std::make_unique<TestHealthCheck>(
                                       HealthStatus::DEGRADED, "Degraded"));

  HealthStatus overall = registry.overall_status();
  EXPECT_EQ(overall, HealthStatus::DEGRADED); // Worst case

  std::string json = registry.status_json();
  EXPECT_NE(json.find("degraded"), std::string::npos);
  EXPECT_NE(json.find("test1"), std::string::npos);
  EXPECT_NE(json.find("test2"), std::string::npos);
}

TEST_F(MonitoringSystemTest, HealthCheckUnhealthy) {
  class UnhealthyCheck : public HealthCheck {
  public:
    HealthStatus check() const override { return HealthStatus::UNHEALTHY; }

    std::string message() const override { return "System failure"; }
  };

  auto &registry = HealthRegistry::instance();
  registry.register_check("critical", std::make_unique<UnhealthyCheck>());

  HealthStatus overall = registry.overall_status();
  EXPECT_EQ(overall, HealthStatus::UNHEALTHY);

  std::string json = registry.status_json();
  EXPECT_NE(json.find("unhealthy"), std::string::npos);
}

// Test multiple metrics
TEST_F(MonitoringSystemTest, MultipleMetrics) {
  auto &counter1 = MetricsRegistry::instance().counter("counter1");
  auto &counter2 = MetricsRegistry::instance().counter("counter2");
  auto &gauge1 = MetricsRegistry::instance().gauge("gauge1");

  counter1.increment(5);
  counter2.increment(10);
  gauge1.set(100);

  std::string prometheus = MetricsRegistry::instance().to_prometheus();

  EXPECT_NE(prometheus.find("pinba_counter1"), std::string::npos);
  EXPECT_NE(prometheus.find("pinba_counter2"), std::string::npos);
  EXPECT_NE(prometheus.find("pinba_gauge1"), std::string::npos);
}
