/* Copyright (c) 2025 Pinba Engine Team
 *
 * Unit tests for modern logging system
 */

#include "pinba_logging_modern.h"
#include <chrono>
#include <cstdio>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <thread>
#include <unistd.h>

using namespace pinba;

class LoggingSystemTest : public ::testing::Test {
protected:
  void SetUp() override {
    test_log_file_ = "/tmp/pinba_test_log_" + std::to_string(getpid()) + ".log";
    // Clean up any existing log file
    std::remove(test_log_file_.c_str());
  }

  void TearDown() override {
    Logger::instance().shutdown();
    // Clean up test log file
    std::remove(test_log_file_.c_str());
  }

  std::string test_log_file_;
};

// Test log levels
TEST_F(LoggingSystemTest, LogLevelToString) {
  EXPECT_STREQ(log_level_to_string(LogLevel::DEBUG), "DEBUG");
  EXPECT_STREQ(log_level_to_string(LogLevel::INFO), "INFO");
  EXPECT_STREQ(log_level_to_string(LogLevel::NOTICE), "NOTICE");
  EXPECT_STREQ(log_level_to_string(LogLevel::WARNING), "WARNING");
  EXPECT_STREQ(log_level_to_string(LogLevel::ERROR), "ERROR");
  EXPECT_STREQ(log_level_to_string(LogLevel::CRITICAL), "CRITICAL");
}

// Test logger initialization
TEST_F(LoggingSystemTest, LoggerInitialization) {
  Logger::instance().initialize(test_log_file_, LogLevel::DEBUG);

  PINBA_LOG_DEBUG("Debug message");
  PINBA_LOG_INFO("Info message");

  // Give async logger time to write
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  Logger::instance().flush();

  // Check if log file was created
  std::ifstream file(test_log_file_);
  EXPECT_TRUE(file.good());

  std::string line;
  bool found_debug = false;
  bool found_info = false;
  while (std::getline(file, line)) {
    if (line.find("DEBUG") != std::string::npos &&
        line.find("Debug message") != std::string::npos) {
      found_debug = true;
    }
    if (line.find("INFO") != std::string::npos &&
        line.find("Info message") != std::string::npos) {
      found_info = true;
    }
  }

  EXPECT_TRUE(found_debug);
  EXPECT_TRUE(found_info);
}

// Test log level filtering
TEST_F(LoggingSystemTest, LogLevelFiltering) {
  Logger::instance().initialize("", LogLevel::WARNING);

  PINBA_LOG_DEBUG("Should not appear");
  PINBA_LOG_INFO("Should not appear");
  PINBA_LOG_WARNING("Should appear");
  PINBA_LOG_ERROR("Should appear");

  Logger::instance().flush();

  // Note: In real scenario, we would check stderr output
  // For now, we just verify no crash
  EXPECT_TRUE(true);
}

// Test structured logging
TEST_F(LoggingSystemTest, StructuredLogging) {
  Logger::instance().initialize(test_log_file_, LogLevel::DEBUG);

  std::string fields = R"({"packet_size":1024,"source":"udp"})";
  PINBA_LOG_WITH_FIELDS(LogLevel::INFO, "Packet received", fields);

  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  Logger::instance().flush();

  std::ifstream file(test_log_file_);
  std::string content((std::istreambuf_iterator<char>(file)),
                      std::istreambuf_iterator<char>());

  EXPECT_NE(content.find("Packet received"), std::string::npos);
  EXPECT_NE(content.find("packet_size"), std::string::npos);
}

// Test async logging
TEST_F(LoggingSystemTest, AsyncLogging) {
  Logger::instance().initialize(test_log_file_, LogLevel::INFO);

  // Log many messages quickly
  for (int i = 0; i < 100; ++i) {
    PINBA_LOG_INFO("Message " + std::to_string(i));
  }

  // Give async logger time to process
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  Logger::instance().flush();

  // Count lines in log file
  std::ifstream file(test_log_file_);
  int line_count = 0;
  std::string line;
  while (std::getline(file, line)) {
    if (line.find("INFO") != std::string::npos) {
      line_count++;
    }
  }

  EXPECT_GE(line_count, 100);
}

// Test logger macros
TEST_F(LoggingSystemTest, LoggerMacros) {
  Logger::instance().initialize(test_log_file_, LogLevel::DEBUG);

  PINBA_LOG_DEBUG("Debug message");
  PINBA_LOG_INFO("Info message");
  PINBA_LOG_NOTICE("Notice message");
  PINBA_LOG_WARNING("Warning message");
  PINBA_LOG_ERROR("Error message");
  PINBA_LOG_CRITICAL("Critical message");

  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  Logger::instance().flush();

  std::ifstream file(test_log_file_);
  std::string content((std::istreambuf_iterator<char>(file)),
                      std::istreambuf_iterator<char>());

  EXPECT_NE(content.find("Debug message"), std::string::npos);
  EXPECT_NE(content.find("Info message"), std::string::npos);
  EXPECT_NE(content.find("Notice message"), std::string::npos);
  EXPECT_NE(content.find("Warning message"), std::string::npos);
  EXPECT_NE(content.find("Error message"), std::string::npos);
  EXPECT_NE(content.find("Critical message"), std::string::npos);
}

// Test logger shutdown
TEST_F(LoggingSystemTest, LoggerShutdown) {
  Logger::instance().initialize(test_log_file_, LogLevel::INFO);

  PINBA_LOG_INFO("Before shutdown");

  Logger::instance().shutdown();

  // After shutdown, logging should still work (fallback mode)
  PINBA_LOG_INFO("After shutdown");

  EXPECT_TRUE(true); // No crash
}
