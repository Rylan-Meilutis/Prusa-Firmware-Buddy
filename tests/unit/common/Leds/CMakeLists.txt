add_executable(startup_activity_tests ${CMAKE_CURRENT_SOURCE_DIR}/startup_activity_tests.cpp)
target_include_directories(startup_activity_tests PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_catch_test(startup_activity_tests)

add_executable(frame_animation_tests ${CMAKE_CURRENT_SOURCE_DIR}/frame_animation_tests.cpp)
target_include_directories(
  frame_animation_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/module/leds/led_animation_controller/include
                                ${CMAKE_SOURCE_DIR}/src/freertos/include
  )
target_link_libraries(frame_animation_tests buddy_utils)
add_catch_test(frame_animation_tests)
