add_executable(st25dv64k_tests ${CMAKE_SOURCE_DIR}/src/common/st25dv64k.cpp st25dv64k_test.cpp)
target_include_directories(
  st25dv64k_tests PRIVATE include ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/common
                          ${CMAKE_SOURCE_DIR}/src/common/utils ${CMAKE_SOURCE_DIR}/tests/stubs
  )
target_link_libraries(st25dv64k_tests freertos_tests)
add_catch_test(st25dv64k_tests)
