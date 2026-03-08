# Tests CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

# Find Google Test
find_package(GTest REQUIRED)

# Test sources
set(TEST_SOURCES
    test_pinba_engine.cc test_error_system.cc test_logging_system.cc
    test_monitoring_system.cc test_integration.cc)

# Create test executable
add_executable(pinba_tests ${TEST_SOURCES})

# Link libraries
target_link_libraries(
  pinba_tests
  pinba_engine
  GTest::GTest
  GTest::Main
  ${MYSQL_LIBRARY}
  ${Protobuf_LIBRARIES}
  ${M_LIBRARY}
  ${PTHREAD_LIBRARY}
  Threads::Threads)

# Include directories
target_include_directories(
  pinba_tests
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src
          ${CMAKE_CURRENT_SOURCE_DIR}/../sparsehash/src ${MYSQL_INCLUDE_DIR}
          ${Protobuf_INCLUDE_DIRS})

# Add tests
add_test(NAME PinbaEngineTests COMMAND pinba_tests)
add_test(NAME PinbaErrorSystemTests COMMAND pinba_tests
                                            --gtest_filter="ErrorSystemTest.*")
add_test(NAME PinbaLoggingSystemTests
         COMMAND pinba_tests --gtest_filter="LoggingSystemTest.*")
add_test(NAME PinbaMonitoringSystemTests
         COMMAND pinba_tests --gtest_filter="MonitoringSystemTest.*")
add_test(NAME PinbaIntegrationTests COMMAND pinba_tests
                                            --gtest_filter="IntegrationTest.*")

# Set test properties
set_tests_properties(PinbaEngineTests PROPERTIES TIMEOUT 300 WORKING_DIRECTORY
                                                 ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(
  PinbaErrorSystemTests PROPERTIES TIMEOUT 300 WORKING_DIRECTORY
                                   ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(
  PinbaLoggingSystemTests PROPERTIES TIMEOUT 300 WORKING_DIRECTORY
                                     ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(
  PinbaMonitoringSystemTests PROPERTIES TIMEOUT 300 WORKING_DIRECTORY
                                        ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(
  PinbaIntegrationTests PROPERTIES TIMEOUT 300 WORKING_DIRECTORY
                                   ${CMAKE_CURRENT_BINARY_DIR})
