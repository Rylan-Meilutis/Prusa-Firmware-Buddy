add_executable(tool_index_iterator_tests tool_index_iterator_tests.cpp)
target_include_directories(
  tool_index_iterator_tests PRIVATE stub_include ${CMAKE_SOURCE_DIR}/src/common
                                    ${CMAKE_SOURCE_DIR}/include
  )
target_link_libraries(tool_index_iterator_tests buddy_utils)
add_catch_test(tool_index_iterator_tests)
