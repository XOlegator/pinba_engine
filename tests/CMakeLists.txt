# Engine sources with no dependency on the running MySQL server. Compiling them
# into the test binary lets the unit tests exercise them directly (and, under the
# coverage preset, report real coverage for these translation units instead of
# the 0% they show when they are only ever linked into the never-executed plugin).
set(PINBA_TESTABLE_ENGINE_SOURCES
    ${CMAKE_SOURCE_DIR}/src/pinba_lmap.cc
    ${CMAKE_SOURCE_DIR}/src/pinba_map.cc
    ${CMAKE_SOURCE_DIR}/src/xxhash.c
    ${CMAKE_SOURCE_DIR}/src/threadpool.cc
    ${CMAKE_SOURCE_DIR}/src/pinba.pb-c.cc
    ${PINBA_PROTO_SRCS})

set(PINBA_TEST_SOURCES
    test_pinba_engine.cc
    test_error_system.cc
    test_logging_system.cc
    test_monitoring_system.cc
    test_integration.cc
    test_pinba_map.cc
    test_threadpool.cc
    test_pinba_protobuf.cc)

add_executable(pinba_tests ${PINBA_TEST_SOURCES} ${PINBA_TESTABLE_ENGINE_SOURCES})

# test_pinba_protobuf.cc and pinba.pb-c.cc include the protobuf-generated
# pinba.pb.h; ensure code generation runs before any test object compiles so a
# clean parallel build cannot race ahead of it.
add_dependencies(pinba_tests pinba_proto_generated)
target_link_libraries(
  pinba_tests
  PRIVATE pinba_project_options
          pinba_includes
          GTest::gtest_main
          ${M_LIBRARY}
          Threads::Threads
          ${Protobuf_LIBRARIES})
target_include_directories(pinba_tests PRIVATE ${CMAKE_BINARY_DIR})

include(GoogleTest)
gtest_discover_tests(
  pinba_tests
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  DISCOVERY_TIMEOUT 60
  PROPERTIES TIMEOUT 300)
