/* Copyright (c) 2025 Pinba Engine Team
 *
 * Integration tests for Pinba Engine components
 */

#include "pinba_error_modern.h"
#include "pinba_logging_modern.h"
#include "pinba_monitoring_modern.h"
#include <chrono>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

using namespace pinba;

class IntegrationTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Initialize logging
    Logger::instance().initialize("", LogLevel::INFO);

    MetricsRegistry::instance().clear();
    HealthRegistry::instance().clear();
  }

  void TearDown() override {
    Logger::instance().shutdown();
    MetricsRegistry::instance().clear();
    HealthRegistry::instance().clear();
  }
};

// Test error handling with logging
TEST_F(IntegrationTest, ErrorHandlingWithLogging) {
  try {
    throw PinbaException(PinbaErrorCode::INVALID_PACKET, "Test error");
  } catch (const PinbaException &e) {
    PINBA_LOG_ERROR("Caught exception: " + std::string(e.what()));

    auto &error_counter = MetricsRegistry::instance().counter("test_errors");
    error_counter.increment();

    EXPECT_EQ(e.code(), PinbaErrorCode::INVALID_PACKET);
    EXPECT_EQ(error_counter.get(), 1);
  }
}

// Test Result type with logging
TEST_F(IntegrationTest, ResultTypeWithLogging) {
  auto operation = []() -> Result<int> {
    // Simulate operation that can fail
    return Result<int>::error(PinbaErrorCode::MEMORY_ALLOCATION_FAILED,
                              "Memory allocation failed");
  };

  auto result = operation();

  if (result.is_error()) {
    PINBA_LOG_WARNING("Operation failed: " + result.error_message());

    auto &failures = MetricsRegistry::instance().counter("operation_failures");
    failures.increment();

    EXPECT_EQ(failures.get(), 1);
  }
}

// Test metrics with error handling
TEST_F(IntegrationTest, MetricsWithErrorHandling) {
  auto &packets_received =
      MetricsRegistry::instance().counter("packets_received");
  auto &packets_errors = MetricsRegistry::instance().counter("packets_errors");
  auto &active_connections =
      MetricsRegistry::instance().gauge("active_connections");

  // Simulate packet processing
  for (int i = 0; i < 10; ++i) {
    try {
      // Simulate packet processing
      packets_received.increment();
      active_connections.set(i + 1);

      if (i == 5) {
        throw PinbaException(PinbaErrorCode::INVALID_PACKET, "Invalid packet");
      }
    } catch (const PinbaException &e) {
      packets_errors.increment();
      PINBA_LOG_WARNING("Packet processing error: " + std::string(e.what()));
    }
  }

  EXPECT_EQ(packets_received.get(), 10);
  EXPECT_EQ(packets_errors.get(), 1);
  EXPECT_EQ(active_connections.get(), 10);
}

// Test health checks with metrics
TEST_F(IntegrationTest, HealthChecksWithMetrics) {
  class SystemHealthCheck : public HealthCheck {
  public:
    SystemHealthCheck() {
      auto &registry = MetricsRegistry::instance();
      errors_ = &registry.counter("system_errors");
      warnings_ = &registry.counter("system_warnings");
    }

    HealthStatus check() const override {
      if (errors_->get() > 10) {
        return HealthStatus::UNHEALTHY;
      }
      if (warnings_->get() > 50) {
        return HealthStatus::DEGRADED;
      }
      return HealthStatus::HEALTHY;
    }

    std::string message() const override {
      return "System status: errors=" + std::to_string(errors_->get()) +
             ", warnings=" + std::to_string(warnings_->get());
    }

  private:
    CounterMetric *errors_;
    CounterMetric *warnings_;
  };

  auto &registry = HealthRegistry::instance();
  registry.register_check("system", std::make_unique<SystemHealthCheck>());

  // Initially healthy
  EXPECT_EQ(registry.overall_status(), HealthStatus::HEALTHY);

  // Add warnings
  auto &warnings = MetricsRegistry::instance().counter("system_warnings");
  for (int i = 0; i < 51; ++i) {
    warnings.increment();
  }

  // Should be degraded
  EXPECT_EQ(registry.overall_status(), HealthStatus::DEGRADED);

  // Add errors
  auto &errors = MetricsRegistry::instance().counter("system_errors");
  for (int i = 0; i < 11; ++i) {
    errors.increment();
  }

  // Should be unhealthy
  EXPECT_EQ(registry.overall_status(), HealthStatus::UNHEALTHY);
}

// Test concurrent operations
TEST_F(IntegrationTest, ConcurrentOperations) {
  auto &counter = MetricsRegistry::instance().counter("concurrent_ops");
  auto &gauge = MetricsRegistry::instance().gauge("concurrent_gauge");

  const int num_threads = 10;
  const int operations_per_thread = 100;

  std::vector<std::thread> threads;

  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([&counter, &gauge, operations_per_thread, i]() {
      for (int j = 0; j < operations_per_thread; ++j) {
        counter.increment();
        gauge.set(i * operations_per_thread + j);

        if (j % 10 == 0) {
          PINBA_LOG_DEBUG("Thread " + std::to_string(i) + " operation " +
                          std::to_string(j));
        }
      }
    });
  }

  for (auto &t : threads) {
    t.join();
  }

  EXPECT_EQ(counter.get(), num_threads * operations_per_thread);
}

// Test error recovery
TEST_F(IntegrationTest, ErrorRecovery) {
  auto &retries = MetricsRegistry::instance().counter("operation_retries");
  auto &successes = MetricsRegistry::instance().counter("operation_successes");

  auto operation = [&retries, &successes](int attempt) -> Result<int> {
    if (attempt < 3) {
      retries.increment();
      return Result<int>::error(PinbaErrorCode::NETWORK_ERROR,
                                "Network error, retrying...");
    }
    successes.increment();
    return Result<int>::success(42);
  };

  int attempt = 0;
  Result<int> result;

  do {
    result = operation(attempt++);
    if (result.is_error()) {
      PINBA_LOG_WARNING("Operation failed, retrying: " +
                        result.error_message());
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  } while (result.is_error() && attempt < 5);

  EXPECT_TRUE(result.is_success());
  EXPECT_EQ(result.value(), 42);
  EXPECT_EQ(retries.get(), 3);
  EXPECT_EQ(successes.get(), 1);
}

// Test Prometheus export with real data
TEST_F(IntegrationTest, PrometheusExportIntegration) {
  auto &packets = MetricsRegistry::instance().counter("packets_total");
  auto &errors = MetricsRegistry::instance().counter("errors_total");
  auto &active = MetricsRegistry::instance().gauge("active_connections");

  packets.increment(1000);
  errors.increment(5);
  active.set(42);

  std::string prometheus = MetricsRegistry::instance().to_prometheus();

  // Verify all metrics are present
  EXPECT_NE(prometheus.find("pinba_packets_total"), std::string::npos);
  EXPECT_NE(prometheus.find("pinba_errors_total"), std::string::npos);
  EXPECT_NE(prometheus.find("pinba_active_connections"), std::string::npos);

  // Verify values
  EXPECT_NE(prometheus.find("1000"), std::string::npos);
  EXPECT_NE(prometheus.find("5"), std::string::npos);
  EXPECT_NE(prometheus.find("42"), std::string::npos);

  // Verify Prometheus format
  EXPECT_NE(prometheus.find("# TYPE"), std::string::npos);
  EXPECT_NE(prometheus.find("counter"), std::string::npos);
  EXPECT_NE(prometheus.find("gauge"), std::string::npos);
}
