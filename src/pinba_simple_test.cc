/* Copyright (c) 2025 Pinba Engine Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 */

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>
#include <optional>

#include "pinba.h"

pinba_daemon *D = nullptr;

static void test_compat_buffers() {
    char small[5];
    size_t len = pinba_copy_truncated(small, sizeof(small), "abcdef", 6);
    if (len != 4 || std::string(small) != "abcd") {
        std::cerr << "pinba_copy_truncated failed" << std::endl;
        std::terminate();
    }

    char concat[8] = "foo";
    len = pinba_append_truncated(concat, sizeof(concat), 3, "123456", 6);
    if (std::string(concat) != "foo1234" || len != 7) {
        std::cerr << "pinba_append_truncated failed" << std::endl;
        std::terminate();
    }
}

// Simple test to verify the configured modern C++ standard
int main() {
    std::cout << "Pinba Engine Modern C++ Test" << std::endl;
    
    // Test modern C++ features
    auto start_time = std::chrono::steady_clock::now();

    test_compat_buffers();
    
    // Test std::optional
    std::optional<int> value = 42;
    if (value) {
        std::cout << "Optional value: " << *value << std::endl;
    }
    
    // Test std::string_view
    std::string str = "Hello, World!";
    std::string_view view = str;
    std::cout << "String view: " << view << std::endl;
    
    // Test modern containers
    std::vector<std::string> items = {"item1", "item2", "item3"};
    for (const auto& item : items) {
        std::cout << "Item: " << item << std::endl;
    }
    
    // Test smart pointers
    auto ptr = std::make_unique<int>(100);
    std::cout << "Smart pointer value: " << *ptr << std::endl;
    
    // Test atomic operations
    std::atomic<int> counter{0};
    counter.fetch_add(1);
    std::cout << "Atomic counter: " << counter.load() << std::endl;
    
    // Test threading primitives availability
    std::thread::id tid = std::this_thread::get_id();
    std::cout << "Thread id available: " << tid << std::endl;
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    std::cout << "Test completed in " << duration.count() << " microseconds" << std::endl;
    std::cout << "Modern C++ features working correctly!" << std::endl;
    
    return 0;
}
