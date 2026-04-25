set(PINBA_BENCHMARK_SOURCES
    benchmark_error_system.cc
    benchmark_logging_system.cc
    benchmark_monitoring_system.cc)

foreach(source IN LISTS PINBA_BENCHMARK_SOURCES)
  get_filename_component(target_name "${source}" NAME_WE)
  add_executable(${target_name} ${source})
  target_link_libraries(
    ${target_name}
    PRIVATE pinba_project_options pinba_includes benchmark::benchmark_main
            ${M_LIBRARY} Threads::Threads ${Protobuf_LIBRARIES})
  target_include_directories(${target_name} PRIVATE ${CMAKE_BINARY_DIR})
endforeach()
