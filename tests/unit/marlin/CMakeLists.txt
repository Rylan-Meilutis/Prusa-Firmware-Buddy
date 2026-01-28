add_executable(
  marlin_temptable_tests
  ${CMAKE_SOURCE_DIR}/lib/Marlin/Marlin/src/module/temperature/marlin_temptable.cpp
  marlin_temptable_tests.cpp
  )
target_include_directories(
  marlin_temptable_tests PRIVATE stub_include_temptable ${CMAKE_SOURCE_DIR}/lib/Marlin/Marlin/src
  )
add_catch_test(marlin_temptable_tests)
