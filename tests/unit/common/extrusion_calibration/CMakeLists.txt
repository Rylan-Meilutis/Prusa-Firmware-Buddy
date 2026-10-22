add_executable(
  extrusion_calibration_tests ${CMAKE_SOURCE_DIR}/src/common/feature/extrusion_calibration.cpp
                              tests.cpp
  )
target_include_directories(extrusion_calibration_tests PUBLIC ${CMAKE_SOURCE_DIR}/src/common)
target_include_directories(
  extrusion_calibration_tests BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  )
add_catch_test(extrusion_calibration_tests)
