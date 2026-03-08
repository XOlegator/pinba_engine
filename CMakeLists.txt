cmake_minimum_required(VERSION 3.16)

# Project information
project(
  pinba_engine
  VERSION 2.0.0
  DESCRIPTION
    "Pinba Engine - MySQL Storage Engine for PHP Performance Statistics"
  LANGUAGES C CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler-specific options
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
  set(CMAKE_CXX_FLAGS_DEBUG "-g3 -O0 -DDEBUG")
  set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
  set(CMAKE_CXX_FLAGS_DEBUG "-g3 -O0 -DDEBUG")
  set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
endif()

# Build type
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Options
option(PINBA_DEBUG "Enable debug output" OFF)
option(PINBA_STATIC_BUILD "Build static library" OFF)
option(PINBA_WITH_TESTS "Build tests" ON)
option(PINBA_WITH_BENCHMARKS "Build benchmarks" OFF)
option(PINBA_ENABLE_SANITIZERS "Enable ASan/UBSan for debug hardening" OFF)
set(
  PINBA_MYSQL_SOURCE_DIR
  ""
  CACHE PATH "Path to MySQL source tree with sql/handler.h")

# Find required packages
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

# Find MySQL 8.0
find_path(
  MYSQL_INCLUDE_DIR
  NAMES mysql.h
  PATHS /usr/include/mysql /usr/local/include/mysql /opt/mysql/include
        /usr/include/mysql8.0 /usr/local/mysql/include /tmp/mysql-8.0/include
  PATH_SUFFIXES mysql)

set(PINBA_MYSQL_SOURCE_ARCHIVE "/usr/src/mysql/mysql-source-8.0.tar.gz")
set(PINBA_MYSQL_SOURCE_EXTRACTED_DIR
    "${CMAKE_BINARY_DIR}/mysql-source/mysql-8.0")

if(NOT PINBA_MYSQL_SOURCE_DIR AND EXISTS "${PINBA_MYSQL_SOURCE_ARCHIVE}")
  file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/mysql-source")
  if(NOT EXISTS "${PINBA_MYSQL_SOURCE_EXTRACTED_DIR}/sql/handler.h")
    execute_process(
      COMMAND ${CMAKE_COMMAND} -E tar xf "${PINBA_MYSQL_SOURCE_ARCHIVE}"
      WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/mysql-source"
      RESULT_VARIABLE PINBA_MYSQL_EXTRACT_RESULT)
    if(NOT PINBA_MYSQL_EXTRACT_RESULT EQUAL 0)
      message(FATAL_ERROR "Failed to extract ${PINBA_MYSQL_SOURCE_ARCHIVE}")
    endif()
  endif()
  set(PINBA_MYSQL_SOURCE_DIR "${PINBA_MYSQL_SOURCE_EXTRACTED_DIR}")
endif()

# Find MySQL 8.0 SQL headers from extracted server source tree.
find_path(
  MYSQL_SQL_INCLUDE_DIR
  NAMES sql/handler.h
  PATHS ${PINBA_MYSQL_SOURCE_DIR}
        /tmp/mysql-8.0-src/mysql-8.0
        /tmp/mysql-8.0
        /usr/include/mysql
        /usr/local/include/mysql
        /usr/local/mysql/include
        /opt/mysql/include
  NO_DEFAULT_PATH)

if(NOT MYSQL_SQL_INCLUDE_DIR)
  message(
    FATAL_ERROR
      "MySQL server headers not found. Set PINBA_MYSQL_SOURCE_DIR "
      "to an extracted mysql-source-8.0 tree.")
endif()

message(STATUS "Found MySQL SQL headers: ${MYSQL_SQL_INCLUDE_DIR}")

find_library(
  MYSQL_LIBRARY
  NAMES mysqlclient
  PATHS /usr/lib/x86_64-linux-gnu /usr/lib /usr/local/lib /opt/mysql/lib
        /usr/lib/mysql8.0 /usr/local/mysql/lib
  PATH_SUFFIXES mysql)

# Check MySQL version
if(MYSQL_INCLUDE_DIR)
  file(READ "${MYSQL_INCLUDE_DIR}/mysql_version.h" MYSQL_VERSION_H)
  string(REGEX MATCH "#define MYSQL_VERSION_ID ([0-9]+)" MYSQL_VERSION_MATCH
               "${MYSQL_VERSION_H}")
  if(MYSQL_VERSION_MATCH)
    set(MYSQL_VERSION_ID ${CMAKE_MATCH_1})
    if(MYSQL_VERSION_ID LESS 80000)
      message(
        FATAL_ERROR
          "MySQL 8.0 or higher is required. Found version: ${MYSQL_VERSION_ID}")
    endif()
    message(STATUS "Found MySQL version: ${MYSQL_VERSION_ID}")
  endif()
endif()

if(NOT MYSQL_INCLUDE_DIR OR NOT MYSQL_LIBRARY)
  message(
    FATAL_ERROR
      "MySQL 8.0 not found. Please install MySQL 8.0 development packages.")
endif()

# Find Protocol Buffers
find_package(Protobuf REQUIRED)
if(Protobuf_VERSION VERSION_LESS "3.0.0")
  message(FATAL_ERROR "Protocol Buffers 3.0.0 or higher is required")
endif()
include_directories(${Protobuf_INCLUDE_DIRS})

set(PINBA_PROTO ${CMAKE_CURRENT_SOURCE_DIR}/pinba.proto)
protobuf_generate_cpp(PINBA_PROTO_SRCS PINBA_PROTO_HDRS ${PINBA_PROTO})
set_source_files_properties(${PINBA_PROTO_SRCS} ${PINBA_PROTO_HDRS}
                            PROPERTIES GENERATED TRUE)

# Find additional libraries
find_library(M_LIBRARY m)
find_library(PTHREAD_LIBRARY pthread)

# Include directories
set(INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/sparsehash/src
    ${MYSQL_INCLUDE_DIR} ${Protobuf_INCLUDE_DIRS})
list(
  APPEND
  INCLUDE_DIRS
  ${MYSQL_SQL_INCLUDE_DIR}
  ${MYSQL_SQL_INCLUDE_DIR}/include
  ${MYSQL_SQL_INCLUDE_DIR}/builddir
  ${MYSQL_SQL_INCLUDE_DIR}/builddir/include
  ${MYSQL_SQL_INCLUDE_DIR}/sql)
include_directories(${INCLUDE_DIRS})

# Compiler definitions
add_definitions(-DMYSQL_DYNAMIC_PLUGIN -DHAVE_CONFIG_H)

if(PINBA_DEBUG)
  add_definitions(-DPINBA_DEBUG -DDEBUG_ON)
else()
  add_definitions(-DNDEBUG -DDEBUG_OFF)
endif()

if(PINBA_ENABLE_SANITIZERS)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
  else()
    message(WARNING "Sanitizers are only supported with GCC/Clang")
  endif()
endif()

# Source files - real Pinba storage engine built against MySQL server headers.
set(PINBA_SOURCES
    src/xxhash.c
    src/pinba.pb-c.cc
    ${PINBA_PROTO_SRCS}
    src/ha_pinba.cc
    src/main.cc
    src/data.cc
    src/pool.cc
    src/tags.cc
    src/threadpool.cc
    src/pinba_map.cc
    src/pinba_lmap.cc)

# Header files
set(PINBA_HEADERS
    src/ha_pinba.h
    src/pinba.h
    src/pinba_types.h
    src/pinba_limits.h
    src/pinba.pb-c.h
    ${PINBA_PROTO_HDRS}
    src/threadpool.h
    src/protobuf-c.h
    src/xxhash.h
    src/pinba_map.h
    src/pinba_lmap.h)

# Generated files
set(PINBA_GENERATED_HEADERS
    src/pinba_regenerate_report.h src/pinba_regenerate_report_tpl.h
    src/pinba_update_report.h src/pinba_update_report_tpl.h
    src/pinba_update_report_proto.h src/pinba_update_report_proto_tpl.h)

# Create the Pinba Storage Engine shared library
add_library(pinba_engine MODULE ${PINBA_SOURCES})

# Set target properties for MySQL Storage Engine
set_target_properties(
  pinba_engine
  PROPERTIES OUTPUT_NAME "ha_pinba"
             PREFIX ""
             SUFFIX ".so"
             VERSION ${PROJECT_VERSION}
             SOVERSION ${PROJECT_VERSION_MAJOR}
             CXX_VISIBILITY_PRESET hidden
             VISIBILITY_INLINES_HIDDEN ON)

# Link libraries for Storage Engine
target_link_libraries(pinba_engine ${MYSQL_LIBRARY} ${M_LIBRARY}
                      ${PTHREAD_LIBRARY} Threads::Threads ${Protobuf_LIBRARIES})
# Export MySQL plugin descriptor symbols (required for INSTALL PLUGIN)
target_include_directories(pinba_engine PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

# Create the test executable
add_executable(pinba_test src/pinba_simple_test.cc)

# Set target properties
set_target_properties(pinba_test PROPERTIES OUTPUT_NAME "pinba_test")

# Link libraries for test
target_link_libraries(pinba_test ${M_LIBRARY} ${PTHREAD_LIBRARY}
                      Threads::Threads ${Protobuf_LIBRARIES})
target_include_directories(pinba_test PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

# Compiler-specific options for the target
target_compile_options(pinba_test PRIVATE $<$<CXX_COMPILER_ID:GNU>:-fPIC>
                                          $<$<CXX_COMPILER_ID:Clang>:-fPIC>)

# Install rules
install(TARGETS pinba_test RUNTIME DESTINATION bin)

install(FILES README default_tables.sql DESTINATION share/pinba_engine)

# Data files
install(
  FILES default_tables.sql
  DESTINATION share/pinba_engine
  RENAME pinba_tables.sql)

# Configuration files
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/config.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/src/config.h @ONLY)

# Generate version info
find_package(Git QUIET)
if(GIT_FOUND)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_SHA1
    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  if(GIT_SHA1)
    string(SUBSTRING ${GIT_SHA1} 0 7 GIT_SHORT_SHA1)
    add_definitions(-DPINBA_GIT_SHA1="${GIT_SHA1}")
    add_definitions(-DPINBA_GIT_SHORT_SHA1="${GIT_SHORT_SHA1}")
  endif()
endif()

# Tests
if(PINBA_WITH_TESTS)
  enable_testing()

  # Find Google Test
  find_package(GTest QUIET)
  if(GTest_FOUND)
    # add_subdirectory(tests)
    message(STATUS "Google Test found but tests disabled for now")
  else()
    message(STATUS "Google Test not found, skipping tests")
  endif()
endif()

# Benchmarks
if(PINBA_WITH_BENCHMARKS)
  find_package(benchmark QUIET)
  if(benchmark_FOUND)
    add_subdirectory(benchmarks)
  else()
    message(STATUS "Google Benchmark not found, skipping benchmarks")
  endif()
endif()

# Package configuration
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
  PinbaEngineConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY AnyNewerVersion)

configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/PinbaEngineConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/PinbaEngineConfig.cmake
  INSTALL_DESTINATION lib/cmake/PinbaEngine)

# Install Storage Engine plugin
install(TARGETS pinba_engine LIBRARY DESTINATION lib/mysql/plugin
                                     COMPONENT Runtime)

# Install configuration files
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/PinbaEngineConfig.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/PinbaEngineConfigVersion.cmake
        DESTINATION lib/cmake/PinbaEngine)

# CPack configuration
set(CPACK_PACKAGE_NAME "pinba-engine")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY ${PROJECT_DESCRIPTION})
set(CPACK_PACKAGE_VENDOR "Pinba Engine Team")
set(CPACK_PACKAGE_CONTACT "pinba-engine@example.com")

include(CPack)

# Print configuration summary
message(STATUS "")
message(STATUS "Pinba Engine Configuration Summary:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  MySQL include: ${MYSQL_INCLUDE_DIR}")
message(STATUS "  MySQL library: ${MYSQL_LIBRARY}")
message(STATUS "  Protocol Buffers: ${Protobuf_VERSION}")
message(STATUS "  Debug mode: ${PINBA_DEBUG}")
message(STATUS "  Static build: ${PINBA_STATIC_BUILD}")
message(STATUS "  Tests: ${PINBA_WITH_TESTS}")
message(STATUS "  Benchmarks: ${PINBA_WITH_BENCHMARKS}")
message(STATUS "")
