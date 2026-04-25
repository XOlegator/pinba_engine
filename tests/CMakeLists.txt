set(PINBA_TEST_SOURCES
    test_pinba_engine.cc
    test_error_system.cc
    test_logging_system.cc
    test_monitoring_system.cc
    test_integration.cc)

add_executable(pinba_tests ${PINBA_TEST_SOURCES})
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
