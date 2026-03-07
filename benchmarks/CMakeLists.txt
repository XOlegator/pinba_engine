# Benchmarks CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

# Find Google Benchmark
find_package(benchmark REQUIRED)

# Benchmark sources
set(BENCHMARK_SOURCES
    benchmark_pool_operations.cc
    benchmark_hash_functions.cc
    benchmark_data_processing.cc
    benchmark_protocol_buffers.cc
    benchmark_error_system.cc
    benchmark_logging_system.cc
    benchmark_monitoring_system.cc)

# Create benchmark executable
add_executable(pinba_benchmarks ${BENCHMARK_SOURCES})

# Link libraries
target_link_libraries(
  pinba_benchmarks
  pinba_engine
  benchmark::benchmark
  benchmark::benchmark_main
  ${MYSQL_LIBRARY}
  ${Protobuf_LIBRARIES}
  ${M_LIBRARY}
  ${PTHREAD_LIBRARY}
  Threads::Threads)

# Include directories
target_include_directories(
  pinba_benchmarks
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src
          ${CMAKE_CURRENT_SOURCE_DIR}/../sparsehash/src ${MYSQL_INCLUDE_DIR}
          ${Protobuf_INCLUDE_DIRS})
