add_executable(rme_protocol_tests rme_protocol_tests.cpp)
target_include_directories(rme_protocol_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/common)
target_include_directories(rme_protocol_tests PRIVATE ${CMAKE_BINARY_DIR}/include)
add_catch_test(rme_protocol_tests)
