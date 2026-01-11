add_executable(signal2step_tests signal2step_tests.cpp)

# Add Marlin stub includes (higher priority) and real Marlin includes
target_include_directories(
  signal2step_tests PRIVATE ${PROJECT_SOURCE_DIR}/tests/stubs/Marlin/src
                            ${PROJECT_SOURCE_DIR}/lib/Marlin/Marlin/src
  )

target_link_libraries(signal2step_tests signal_processing)
add_catch_test(signal2step_tests)
