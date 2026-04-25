/* Copyright (c) 2025 Pinba Engine Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 */

#ifndef PINBA_MONITORING_MODERN_H
#define PINBA_MONITORING_MODERN_H

#include <atomic>
#include <chrono>
#include <string>
#include <unordered_map>
#include <mutex>
#include <vector>
#include <cstdint>

namespace pinba {

// Metric types
enum class MetricType {
    COUNTER,    // Monotonically increasing counter
    GAUGE,      // Value that can go up or down
    HISTOGRAM,  // Distribution of values
    SUMMARY     // Summary statistics
};

// Base metric interface
class Metric {
public:
    virtual ~Metric() = default;
    virtual std::string to_prometheus(const std::string& name, const std::string& labels = "") const = 0;
    virtual MetricType type() const = 0;
};

// Counter metric
class CounterMetric : public Metric {
public:
    CounterMetric() : value_(0) {}
    
    void increment(std::int64_t amount = 1) {
        value_.fetch_add(amount, std::memory_order_relaxed);
    }
    
    void reset() {
        value_.store(0, std::memory_order_relaxed);
    }
    
    std::int64_t get() const {
        return value_.load(std::memory_order_relaxed);
    }
    
    std::string to_prometheus(const std::string& name, const std::string& labels = "") const override {
        std::string result = "# TYPE " + name + " counter\n";
        result += name;
        if (!labels.empty()) {
            result += "{" + labels + "}";
        }
        result += " " + std::to_string(get()) + "\n";
        return result;
    }
    
    MetricType type() const override {
        return MetricType::COUNTER;
    }

private:
    std::atomic<std::int64_t> value_;
};

// Gauge metric
class GaugeMetric : public Metric {
public:
    GaugeMetric() : value_(0) {}
    
    void set(std::int64_t val) {
        value_.store(val, std::memory_order_relaxed);
    }
    
    void increment(std::int64_t amount = 1) {
        value_.fetch_add(amount, std::memory_order_relaxed);
    }
    
    void decrement(std::int64_t amount = 1) {
        value_.fetch_sub(amount, std::memory_order_relaxed);
    }
    
    std::int64_t get() const {
        return value_.load(std::memory_order_relaxed);
    }
    
    std::string to_prometheus(const std::string& name, const std::string& labels = "") const override {
        std::string result = "# TYPE " + name + " gauge\n";
        result += name;
        if (!labels.empty()) {
            result += "{" + labels + "}";
        }
        result += " " + std::to_string(get()) + "\n";
        return result;
    }
    
    MetricType type() const override {
        return MetricType::GAUGE;
    }

private:
    std::atomic<std::int64_t> value_;
};

// Histogram metric (simplified - stores buckets)
class HistogramMetric : public Metric {
public:
    HistogramMetric(const std::vector<double>& buckets) : buckets_(buckets) {
        counts_.resize(buckets.size() + 1, 0); // +1 for +Inf bucket
        sum_ = 0.0;
        count_.store(0, std::memory_order_relaxed);
    }
    
    void observe(double value) {
        std::lock_guard<std::mutex> lock(mutex_);
        sum_ += value;
        count_.fetch_add(1, std::memory_order_relaxed);
        
        // Find bucket
        size_t bucket_idx = buckets_.size();
        for (size_t i = 0; i < buckets_.size(); ++i) {
            if (value <= buckets_[i]) {
                bucket_idx = i;
                break;
            }
        }
        counts_[bucket_idx]++;
    }
    
    std::string to_prometheus(const std::string& name, const std::string& labels = "") const override {
        std::string result = "# TYPE " + name + " histogram\n";
        
        std::lock_guard<std::mutex> lock(mutex_);
        std::int64_t total_count = count_.load(std::memory_order_relaxed);
        double total_sum = sum_;
        
        // Bucket counts
        std::int64_t cumulative = 0;
        for (size_t i = 0; i < buckets_.size(); ++i) {
            cumulative += counts_[i];
            result += name + "_bucket";
            std::string bucket_labels = labels;
            if (!bucket_labels.empty()) bucket_labels += ",";
            bucket_labels += "le=\"" + std::to_string(buckets_[i]) + "\"";
            result += "{" + bucket_labels + "} " + std::to_string(cumulative) + "\n";
        }
        // +Inf bucket
        result += name + "_bucket";
        std::string inf_labels = labels;
        if (!inf_labels.empty()) inf_labels += ",";
        inf_labels += "le=\"+Inf\"";
        result += "{" + inf_labels + "} " + std::to_string(total_count) + "\n";
        
        // Sum and count
        result += name + "_sum";
        if (!labels.empty()) {
            result += "{" + labels + "}";
        }
        result += " " + std::to_string(total_sum) + "\n";
        
        result += name + "_count";
        if (!labels.empty()) {
            result += "{" + labels + "}";
        }
        result += " " + std::to_string(total_count) + "\n";
        
        return result;
    }
    
    MetricType type() const override {
        return MetricType::HISTOGRAM;
    }

private:
    std::vector<double> buckets_;
    std::vector<std::int64_t> counts_;
    double sum_{0.0};
    std::atomic<std::int64_t> count_;
    mutable std::mutex mutex_;
};

// Metrics registry
class MetricsRegistry {
public:
    static MetricsRegistry& instance() {
        static MetricsRegistry registry;
        return registry;
    }
    
    CounterMetric& counter(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = counters_.find(name);
        if (it == counters_.end()) {
            auto metric = std::make_unique<CounterMetric>();
            auto* ptr = metric.get();
            counters_[name] = std::move(metric);
            return *ptr;
        }
        return *it->second;
    }
    
    GaugeMetric& gauge(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = gauges_.find(name);
        if (it == gauges_.end()) {
            auto metric = std::make_unique<GaugeMetric>();
            auto* ptr = metric.get();
            gauges_[name] = std::move(metric);
            return *ptr;
        }
        return *it->second;
    }
    
    HistogramMetric& histogram(const std::string& name, const std::vector<double>& buckets) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = histograms_.find(name);
        if (it == histograms_.end()) {
            auto metric = std::make_unique<HistogramMetric>(buckets);
            auto* ptr = metric.get();
            histograms_[name] = std::move(metric);
            return *ptr;
        }
        return *it->second;
    }
    
    std::string to_prometheus() const {
        std::string result;
        std::lock_guard<std::mutex> lock(mutex_);
        
        for (const auto& [name, metric] : counters_) {
            result += metric->to_prometheus("pinba_" + name);
        }
        
        for (const auto& [name, metric] : gauges_) {
            result += metric->to_prometheus("pinba_" + name);
        }
        
        for (const auto& [name, metric] : histograms_) {
            result += metric->to_prometheus("pinba_" + name);
        }
        
        return result;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        counters_.clear();
        gauges_.clear();
        histograms_.clear();
    }

private:
    std::unordered_map<std::string, std::unique_ptr<CounterMetric>> counters_;
    std::unordered_map<std::string, std::unique_ptr<GaugeMetric>> gauges_;
    std::unordered_map<std::string, std::unique_ptr<HistogramMetric>> histograms_;
    mutable std::mutex mutex_;
};

// Health check status
enum class HealthStatus {
    HEALTHY,
    DEGRADED,
    UNHEALTHY
};

// Health check interface
class HealthCheck {
public:
    virtual ~HealthCheck() = default;
    virtual HealthStatus check() const = 0;
    virtual std::string message() const = 0;
};

// Health check registry
class HealthRegistry {
public:
    static HealthRegistry& instance() {
        static HealthRegistry registry;
        return registry;
    }
    
    void register_check(const std::string& name, std::unique_ptr<HealthCheck> check) {
        std::lock_guard<std::mutex> lock(mutex_);
        checks_[name] = std::move(check);
    }
    
    HealthStatus overall_status() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return overall_status_unlocked();
    }
    
    std::string status_json() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string result = "{\"status\":\"";
        
        HealthStatus overall = overall_status_unlocked();
        switch (overall) {
            case HealthStatus::HEALTHY:
                result += "healthy";
                break;
            case HealthStatus::DEGRADED:
                result += "degraded";
                break;
            case HealthStatus::UNHEALTHY:
                result += "unhealthy";
                break;
        }
        result += "\",\"checks\":{";
        
        bool first = true;
        for (const auto& [name, check] : checks_) {
            if (!first) result += ",";
            first = false;
            
            result += "\"" + name + "\":{";
            result += "\"status\":\"";
            switch (check->check()) {
                case HealthStatus::HEALTHY:
                    result += "healthy";
                    break;
                case HealthStatus::DEGRADED:
                    result += "degraded";
                    break;
                case HealthStatus::UNHEALTHY:
                    result += "unhealthy";
                    break;
            }
            result += "\",\"message\":\"" + check->message() + "\"";
            result += "}";
        }
        
        result += "}}";
        return result;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        checks_.clear();
    }

private:
    HealthStatus overall_status_unlocked() const {
        HealthStatus worst = HealthStatus::HEALTHY;

        for (const auto& [name, check] : checks_) {
            HealthStatus status = check->check();
            if (status > worst) {
                worst = status;
            }
        }

        return worst;
    }

    std::unordered_map<std::string, std::unique_ptr<HealthCheck>> checks_;
    mutable std::mutex mutex_;
};

} // namespace pinba

#endif // PINBA_MONITORING_MODERN_H
