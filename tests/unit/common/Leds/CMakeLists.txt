add_executable(
  leds_tests
  ${CMAKE_SOURCE_DIR}/tests/unit/test_main.cpp ${CMAKE_SOURCE_DIR}/tests/stubs/malloc_stub.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/led_animation_tests.cpp ${CMAKE_CURRENT_SOURCE_DIR}/timing_dummy.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/leds_dummy.cpp ${CMAKE_CURRENT_SOURCE_DIR}/leds_eeprom_access.cpp
  )
target_include_directories(
  leds_tests
  PUBLIC ${CMAKE_SOURCE_DIR}/tests/stubs ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/common
         ${CMAKE_SOURCE_DIR}/src/gui/led_animations ${CMAKE_SOURCE_DIR}/src/guiapi/include
         ${CMAKE_SOURCE_DIR}/src/gui/dialogs
  )
target_link_libraries(leds_tests PRIVATE freertos_tests)

add_executable(frame_animation_tests ${CMAKE_CURRENT_SOURCE_DIR}/frame_animation_tests.cpp)
target_include_directories(
  frame_animation_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/src/module/leds/led_animation_controller/include
          ${CMAKE_SOURCE_DIR}/src/freertos/include
  )
target_link_libraries(frame_animation_tests buddy_utils)
add_catch_test(frame_animation_tests)
