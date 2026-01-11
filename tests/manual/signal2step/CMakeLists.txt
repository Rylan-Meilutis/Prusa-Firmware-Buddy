add_executable(signal2step_test signal2step_test.cpp)

target_include_directories(
  signal2step_test PRIVATE ${PROJECT_SOURCE_DIR}/tests/stubs/Marlin/src
                           ${PROJECT_SOURCE_DIR}/lib/Marlin/Marlin/src
  )

target_link_libraries(signal2step_test signal_processing)
target_compile_definitions(signal2step_test PUBLIC -DUNITTESTS)
add_dependencies(tests signal2step_test)
