/* Copyright (c) 2025 Pinba Engine Team
 *
 * Unit tests for modern error handling system
 */

#include "pinba_error_modern.h"
#include <gtest/gtest.h>
#include <system_error>

using namespace pinba;

class ErrorSystemTest : public ::testing::Test {
protected:
  void SetUp() override {}

  void TearDown() override {}
};

// Test error codes
TEST_F(ErrorSystemTest, ErrorCodeCreation) {
  auto ec = make_error_code(PinbaErrorCode::INVALID_PACKET);
  EXPECT_EQ(ec.value(), static_cast<int>(PinbaErrorCode::INVALID_PACKET));
  EXPECT_STREQ(ec.category().name(), "pinba");
  EXPECT_FALSE(ec.message().empty());
}

TEST_F(ErrorSystemTest, ErrorCodeMessages) {
  auto ec1 = make_error_code(PinbaErrorCode::SUCCESS);
  EXPECT_EQ(ec1.message(), "Success");

  auto ec2 = make_error_code(PinbaErrorCode::MEMORY_ALLOCATION_FAILED);
  EXPECT_EQ(ec2.message(), "Memory allocation failed");

  auto ec3 = make_error_code(PinbaErrorCode::INVALID_PACKET);
  EXPECT_EQ(ec3.message(), "Invalid packet format");
}

// Test exceptions
TEST_F(ErrorSystemTest, PinbaExceptionCreation) {
  PinbaException ex(PinbaErrorCode::INVALID_PACKET, "Test error");
  EXPECT_EQ(ex.code(), PinbaErrorCode::INVALID_PACKET);
  EXPECT_STREQ(ex.what(), "Test error");

  auto ec = ex.error_code();
  EXPECT_EQ(ec.value(), static_cast<int>(PinbaErrorCode::INVALID_PACKET));
}

TEST_F(ErrorSystemTest, PinbaExceptionThrow) {
  EXPECT_THROW(
      { throw PinbaException(PinbaErrorCode::INVALID_PACKET, "Test"); },
      PinbaException);

  EXPECT_THROW(
      { throw PinbaException(PinbaErrorCode::INVALID_PACKET, "Test"); },
      std::exception);
}

TEST_F(ErrorSystemTest, PinbaSystemException) {
  PinbaSystemException ex(PinbaErrorCode::NETWORK_ERROR,
                          "Network operation failed", ENOENT);
  EXPECT_EQ(ex.code(), PinbaErrorCode::NETWORK_ERROR);
  EXPECT_EQ(ex.system_error(), ENOENT);
  EXPECT_NE(std::string(ex.what()).find("Network operation failed"),
            std::string::npos);
}

// Test Result type
TEST_F(ErrorSystemTest, ResultSuccess) {
  auto result = Result<int>::success(42);
  EXPECT_TRUE(result.is_success());
  EXPECT_FALSE(result.is_error());
  EXPECT_EQ(result.value(), 42);
}

TEST_F(ErrorSystemTest, ResultError) {
  auto result =
      Result<int>::error(PinbaErrorCode::INVALID_PACKET, "Invalid packet");
  EXPECT_FALSE(result.is_success());
  EXPECT_TRUE(result.is_error());
  EXPECT_EQ(result.error_code(), PinbaErrorCode::INVALID_PACKET);
  EXPECT_EQ(result.error_message(), "Invalid packet");

  EXPECT_THROW(
      {
        auto val = result.value();
        (void)val;
      },
      PinbaException);
}

TEST_F(ErrorSystemTest, ResultVoidSuccess) {
  auto result = Result<void>::success();
  EXPECT_TRUE(result.is_success());
  EXPECT_FALSE(result.is_error());
}

TEST_F(ErrorSystemTest, ResultVoidError) {
  auto result = Result<void>::error(PinbaErrorCode::MEMORY_ALLOCATION_FAILED,
                                    "Out of memory");
  EXPECT_FALSE(result.is_success());
  EXPECT_TRUE(result.is_error());
  EXPECT_EQ(result.error_code(), PinbaErrorCode::MEMORY_ALLOCATION_FAILED);
}

TEST_F(ErrorSystemTest, ResultMoveSemantics) {
  auto result1 = Result<std::string>::success("test");
  auto result2 = std::move(result1);

  EXPECT_TRUE(result2.is_success());
  EXPECT_EQ(result2.value(), "test");
}

// Test error code registration
TEST_F(ErrorSystemTest, ErrorCodeInStdErrorCode) {
  std::error_code ec = PinbaErrorCode::INVALID_PACKET;
  EXPECT_EQ(ec.value(), static_cast<int>(PinbaErrorCode::INVALID_PACKET));
  EXPECT_STREQ(ec.category().name(), "pinba");
}
