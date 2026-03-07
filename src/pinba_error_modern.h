/* Copyright (c) 2025 Pinba Engine Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 */

#ifndef PINBA_ERROR_MODERN_H
#define PINBA_ERROR_MODERN_H

#include <system_error>
#include <string>
#include <exception>
#include <memory>
#include <cstdint>

namespace pinba {

// Error codes for Pinba Engine
enum class PinbaErrorCode : int {
    SUCCESS = 0,
    INVALID_PACKET = 1,
    MEMORY_ALLOCATION_FAILED = 2,
    POOL_INITIALIZATION_FAILED = 3,
    SOCKET_CREATION_FAILED = 4,
    SOCKET_BIND_FAILED = 5,
    INVALID_CONFIGURATION = 6,
    TAG_CREATION_FAILED = 7,
    REPORT_UPDATE_FAILED = 8,
    PROTOBUF_DECODE_ERROR = 9,
    THREAD_CREATION_FAILED = 10,
    LOCK_ACQUISITION_FAILED = 11,
    INVALID_DATA_FORMAT = 12,
    BUFFER_OVERFLOW = 13,
    NETWORK_ERROR = 14,
    INTERNAL_ERROR = 15
};

// Error category for Pinba errors
class PinbaErrorCategory : public std::error_category {
public:
    const char* name() const noexcept override {
        return "pinba";
    }
    
    std::string message(int ev) const override {
        switch (static_cast<PinbaErrorCode>(ev)) {
            case PinbaErrorCode::SUCCESS:
                return "Success";
            case PinbaErrorCode::INVALID_PACKET:
                return "Invalid packet format";
            case PinbaErrorCode::MEMORY_ALLOCATION_FAILED:
                return "Memory allocation failed";
            case PinbaErrorCode::POOL_INITIALIZATION_FAILED:
                return "Pool initialization failed";
            case PinbaErrorCode::SOCKET_CREATION_FAILED:
                return "Socket creation failed";
            case PinbaErrorCode::SOCKET_BIND_FAILED:
                return "Socket bind failed";
            case PinbaErrorCode::INVALID_CONFIGURATION:
                return "Invalid configuration";
            case PinbaErrorCode::TAG_CREATION_FAILED:
                return "Tag creation failed";
            case PinbaErrorCode::REPORT_UPDATE_FAILED:
                return "Report update failed";
            case PinbaErrorCode::PROTOBUF_DECODE_ERROR:
                return "Protocol buffer decode error";
            case PinbaErrorCode::THREAD_CREATION_FAILED:
                return "Thread creation failed";
            case PinbaErrorCode::LOCK_ACQUISITION_FAILED:
                return "Lock acquisition failed";
            case PinbaErrorCode::INVALID_DATA_FORMAT:
                return "Invalid data format";
            case PinbaErrorCode::BUFFER_OVERFLOW:
                return "Buffer overflow";
            case PinbaErrorCode::NETWORK_ERROR:
                return "Network error";
            case PinbaErrorCode::INTERNAL_ERROR:
                return "Internal error";
            default:
                return "Unknown error";
        }
    }
};

// Get error category instance
inline const PinbaErrorCategory& pinba_error_category() {
    static PinbaErrorCategory instance;
    return instance;
}

// Create error_code from PinbaErrorCode
inline std::error_code make_error_code(PinbaErrorCode e) {
    return std::error_code(static_cast<int>(e), pinba_error_category());
}

// Exception hierarchy
class PinbaException : public std::exception {
public:
    PinbaException(PinbaErrorCode code, const std::string& message)
        : code_(code), message_(message) {}
    
    PinbaException(PinbaErrorCode code, const char* message)
        : code_(code), message_(message) {}
    
    const char* what() const noexcept override {
        return message_.c_str();
    }
    
    PinbaErrorCode code() const noexcept {
        return code_;
    }
    
    std::error_code error_code() const {
        return make_error_code(code_);
    }

private:
    PinbaErrorCode code_;
    std::string message_;
};

class PinbaSystemException : public PinbaException {
public:
    PinbaSystemException(PinbaErrorCode code, const std::string& message, int errno_value)
        : PinbaException(code, message + ": " + std::to_string(errno_value)),
          errno_value_(errno_value) {}
    
    int system_error() const noexcept {
        return errno_value_;
    }

private:
    int errno_value_;
};

// Result type for operations that can fail
template<typename T>
class Result {
public:
    static Result success(T value) {
        Result r;
        r.value_ = std::make_unique<T>(std::move(value));
        return r;
    }
    
    static Result error(PinbaErrorCode code, const std::string& message) {
        Result r;
        r.error_code_ = code;
        r.error_message_ = message;
        return r;
    }
    
    bool is_success() const {
        return value_ != nullptr;
    }
    
    bool is_error() const {
        return value_ == nullptr;
    }
    
    T& value() {
        if (!value_) {
            throw PinbaException(error_code_, error_message_);
        }
        return *value_;
    }
    
    const T& value() const {
        if (!value_) {
            throw PinbaException(error_code_, error_message_);
        }
        return *value_;
    }
    
    PinbaErrorCode error_code() const {
        return error_code_;
    }
    
    const std::string& error_message() const {
        return error_message_;
    }
    
    std::error_code std_error_code() const {
        return make_error_code(error_code_);
    }

private:
    std::unique_ptr<T> value_;
    PinbaErrorCode error_code_ = PinbaErrorCode::SUCCESS;
    std::string error_message_;
};

// Specialization for void
template<>
class Result<void> {
public:
    static Result success() {
        Result r;
        r.success_ = true;
        return r;
    }
    
    static Result error(PinbaErrorCode code, const std::string& message) {
        Result r;
        r.success_ = false;
        r.error_code_ = code;
        r.error_message_ = message;
        return r;
    }
    
    bool is_success() const {
        return success_;
    }
    
    bool is_error() const {
        return !success_;
    }
    
    PinbaErrorCode error_code() const {
        return error_code_;
    }
    
    const std::string& error_message() const {
        return error_message_;
    }
    
    std::error_code std_error_code() const {
        return make_error_code(error_code_);
    }

private:
    bool success_ = false;
    PinbaErrorCode error_code_ = PinbaErrorCode::SUCCESS;
    std::string error_message_;
};

} // namespace pinba

// Register error code for std::error_code
namespace std {
    template<>
    struct is_error_code_enum<pinba::PinbaErrorCode> : true_type {};
}

#endif // PINBA_ERROR_MODERN_H
