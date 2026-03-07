/* Copyright (c) 2025 Pinba Engine Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 */

#ifndef PINBA_LOGGING_MODERN_H
#define PINBA_LOGGING_MODERN_H

#include <string>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <atomic>
#include <chrono>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <ctime>
#include <cstdint>
#include <condition_variable>

namespace pinba {

// Log levels
enum class LogLevel : std::uint8_t {
    DEBUG = 0,
    INFO = 1,
    NOTICE = 2,
    WARNING = 3,
    ERROR = 4,
    CRITICAL = 5
};

// Convert log level to string
inline const char* log_level_to_string(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::NOTICE: return "NOTICE";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

// Log entry structure
struct LogEntry {
    std::chrono::system_clock::time_point timestamp;
    LogLevel level;
    std::string file;
    int line;
    std::string function;
    std::string message;
    std::string json_fields; // Additional JSON fields for structured logging
    
    LogEntry(LogLevel lvl, const char* f, int ln, const char* func, const std::string& msg)
        : timestamp(std::chrono::system_clock::now())
        , level(lvl)
        , file(f)
        , line(ln)
        , function(func ? func : "")
        , message(msg) {}
};

// Asynchronous logger
class AsyncLogger {
public:
    AsyncLogger(const std::string& log_file = "", LogLevel min_level = LogLevel::INFO)
        : log_file_(log_file)
        , min_level_(min_level)
        , running_(true)
        , worker_thread_(&AsyncLogger::worker_loop, this) {}
    
    ~AsyncLogger() {
        stop();
    }
    
    void log(LogLevel level, const char* file, int line, const char* function, const std::string& message) {
        if (level < min_level_) {
            return;
        }
        
        LogEntry entry(level, file, line, function, message);
        
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            log_queue_.push(std::move(entry));
        }
        
        condition_.notify_one();
    }
    
    void log_with_fields(LogLevel level, const char* file, int line, const char* function,
                        const std::string& message, const std::string& json_fields) {
        if (level < min_level_) {
            return;
        }
        
        LogEntry entry(level, file, line, function, message);
        entry.json_fields = json_fields;
        
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            log_queue_.push(std::move(entry));
        }
        
        condition_.notify_one();
    }
    
    void set_min_level(LogLevel level) {
        min_level_ = level;
    }
    
    void stop() {
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            running_ = false;
        }
        condition_.notify_all();
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
    }
    
    void flush() {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        while (!log_queue_.empty()) {
            process_entry(log_queue_.front());
            log_queue_.pop();
        }
    }

private:
    void worker_loop() {
        while (true) {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            condition_.wait(lock, [this] { return !log_queue_.empty() || !running_; });
            
            if (!running_ && log_queue_.empty()) {
                break;
            }
            
            while (!log_queue_.empty()) {
                LogEntry entry = std::move(log_queue_.front());
                log_queue_.pop();
                lock.unlock();
                
                process_entry(entry);
                
                lock.lock();
            }
        }
    }
    
    void process_entry(const LogEntry& entry) {
        std::string formatted = format_entry(entry);
        
        if (!log_file_.empty()) {
            std::ofstream file(log_file_, std::ios::app);
            if (file.is_open()) {
                file << formatted << std::endl;
            }
        } else {
            // Fallback to stderr for compatibility
            fprintf(stderr, "%s\n", formatted.c_str());
            fflush(stderr);
        }
    }
    
    std::string format_entry(const LogEntry& entry) const {
        auto time_t = std::chrono::system_clock::to_time_t(entry.timestamp);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            entry.timestamp.time_since_epoch()) % 1000;
        
        std::ostringstream oss;
        std::tm tm_buf;
        localtime_r(&time_t, &tm_buf);
        
        oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
        oss << "." << std::setfill('0') << std::setw(3) << ms.count();
        oss << " [" << log_level_to_string(entry.level) << "]";
        
        if (!entry.file.empty()) {
            // Extract filename from path
            const char* filename = std::strrchr(entry.file.c_str(), '/');
            filename = filename ? filename + 1 : entry.file.c_str();
            oss << " " << filename << ":" << entry.line;
        }
        
        if (!entry.function.empty()) {
            oss << " " << entry.function << "()";
        }
        
        oss << " " << entry.message;
        
        if (!entry.json_fields.empty()) {
            oss << " " << entry.json_fields;
        }
        
        return oss.str();
    }
    
    std::string log_file_;
    LogLevel min_level_;
    std::atomic<bool> running_;
    std::queue<LogEntry> log_queue_;
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    std::thread worker_thread_;
};

// Global logger instance (thread-safe singleton)
class Logger {
public:
    static Logger& instance() {
        static Logger logger;
        return logger;
    }
    
    void initialize(const std::string& log_file = "", LogLevel min_level = LogLevel::INFO) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (async_logger_) {
            async_logger_->stop();
        }
        async_logger_ = std::make_unique<AsyncLogger>(log_file, min_level);
    }
    
    void log(LogLevel level, const char* file, int line, const char* function, const std::string& message) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (async_logger_) {
            async_logger_->log(level, file, line, function, message);
        } else {
            // Fallback to synchronous logging
            log_sync(level, file, line, function, message);
        }
    }
    
    void log_with_fields(LogLevel level, const char* file, int line, const char* function,
                        const std::string& message, const std::string& json_fields) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (async_logger_) {
            async_logger_->log_with_fields(level, file, line, function, message, json_fields);
        } else {
            log_sync(level, file, line, function, message + " " + json_fields);
        }
    }
    
    void set_min_level(LogLevel level) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (async_logger_) {
            async_logger_->set_min_level(level);
        }
    }
    
    void flush() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (async_logger_) {
            async_logger_->flush();
        }
    }
    
    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (async_logger_) {
            async_logger_->stop();
            async_logger_.reset();
        }
    }

private:
    void log_sync(LogLevel level, const char* file, int line, const char* function, const std::string& message) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::tm tm_buf;
        localtime_r(&time_t, &tm_buf);
        
        char timebuf[64];
        std::strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", &tm_buf);
        
        fprintf(stderr, "[%s] [%s] %s:%d %s() %s\n",
                timebuf, log_level_to_string(level), file, line, function ? function : "", message.c_str());
        fflush(stderr);
    }
    
    std::unique_ptr<AsyncLogger> async_logger_;
    std::mutex mutex_;
};

// Convenience macros for logging
#define PINBA_LOG_DEBUG(msg) \
    pinba::Logger::instance().log(pinba::LogLevel::DEBUG, __FILE__, __LINE__, __FUNCTION__, msg)

#define PINBA_LOG_INFO(msg) \
    pinba::Logger::instance().log(pinba::LogLevel::INFO, __FILE__, __LINE__, __FUNCTION__, msg)

#define PINBA_LOG_NOTICE(msg) \
    pinba::Logger::instance().log(pinba::LogLevel::NOTICE, __FILE__, __LINE__, __FUNCTION__, msg)

#define PINBA_LOG_WARNING(msg) \
    pinba::Logger::instance().log(pinba::LogLevel::WARNING, __FILE__, __LINE__, __FUNCTION__, msg)

#define PINBA_LOG_ERROR(msg) \
    pinba::Logger::instance().log(pinba::LogLevel::ERROR, __FILE__, __LINE__, __FUNCTION__, msg)

#define PINBA_LOG_CRITICAL(msg) \
    pinba::Logger::instance().log(pinba::LogLevel::CRITICAL, __FILE__, __LINE__, __FUNCTION__, msg)

// Structured logging macro
#define PINBA_LOG_WITH_FIELDS(level, msg, fields) \
    pinba::Logger::instance().log_with_fields(level, __FILE__, __LINE__, __FUNCTION__, msg, fields)

} // namespace pinba

#endif // PINBA_LOGGING_MODERN_H
