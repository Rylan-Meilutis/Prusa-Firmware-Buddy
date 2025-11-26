add_library(modbus STATIC src/modbus/modbus.cpp)
target_include_directories(modbus PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
