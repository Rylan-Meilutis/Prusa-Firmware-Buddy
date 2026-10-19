add_executable(rme_protocol_tests pa_fast_search_tests.cpp rme_protocol_tests.cpp)
target_include_directories(rme_protocol_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/common)
target_include_directories(rme_protocol_tests PRIVATE ${CMAKE_BINARY_DIR}/include)
add_catch_test(rme_protocol_tests)

# Run the actual optimizer/burst/batch control code with fault-injected host peripherals, not just
# isolated predicates. Keep it in the normal CTest suite.
add_test(NAME m976_fast_path_faults
         COMMAND "${Python3_EXECUTABLE}"
                 "${CMAKE_SOURCE_DIR}/tests/unit/gui/test_m976_fast_search.py"
         )
set_tests_properties(m976_fast_path_faults PROPERTIES LABELS "rme;m976;fault-injection" TIMEOUT 120)
