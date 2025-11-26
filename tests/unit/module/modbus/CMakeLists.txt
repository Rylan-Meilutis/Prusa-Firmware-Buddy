add_executable(modbus_tests modbus_tests.cpp)
target_link_libraries(modbus_tests modbus)
add_catch_test(modbus_tests)
