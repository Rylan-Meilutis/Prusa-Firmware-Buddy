# All these tests need buddy_utils
link_libraries(buddy_utils)

add_test(NAME firmware_cleanup_recovery_faults
         COMMAND "${Python3_EXECUTABLE}" -m unittest discover -s "${CMAKE_CURRENT_SOURCE_DIR}" -p
                 "test_firmware*py"
         )
set_tests_properties(
  firmware_cleanup_recovery_faults PROPERTIES LABELS "firmware;usb;fault-injection" TIMEOUT 120
  )

add_test(NAME serial_pause_recovery_faults
         COMMAND "${Python3_EXECUTABLE}"
                 "${CMAKE_CURRENT_SOURCE_DIR}/test_serial_pause_recovery.py"
         )
set_tests_properties(
  serial_pause_recovery_faults PROPERTIES LABELS "serial;pause;fault-injection" TIMEOUT 120
  )

add_test(NAME font_render_recovery_faults
         COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_font_render_recovery.py"
         )
set_tests_properties(
  font_render_recovery_faults PROPERTIES LABELS "gui;fonts;fault-injection" TIMEOUT 120
  )

# define the test executable
add_executable(
  rectangle_tests ${CMAKE_SOURCE_DIR}/src/guiapi/src/Rect16.cpp
                  ${CMAKE_SOURCE_DIR}/tests/unit/test_main.cpp rectangle_tests.cpp
  )

add_executable(
  gui_time_tests
  ${CMAKE_SOURCE_DIR}/src/gui/gui_time.cpp ${CMAKE_CURRENT_SOURCE_DIR}/gui_time_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/gui_time_tests.c
  )

add_executable(
  gui_tests ${CMAKE_CURRENT_SOURCE_DIR}/gui_tests.cpp
            ${CMAKE_SOURCE_DIR}/src/guiapi/src/numeric_input_config.cpp
  )

# define required search paths
target_include_directories(
  rectangle_tests
  PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/guiapi/include ${CMAKE_SOURCE_DIR}/src/gui
         ${CMAKE_SOURCE_DIR}/src/common ${CMAKE_SOURCE_DIR}/src/guiconfig
         ${CMAKE_SOURCE_DIR}/include
  )

target_include_directories(
  gui_time_tests PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/common
                        ${CMAKE_SOURCE_DIR}/src/guiapi/include ${CMAKE_CURRENT_SOURCE_DIR}
  )

target_include_directories(
  gui_tests
  PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/common ${CMAKE_SOURCE_DIR}/src/lang
         ${CMAKE_SOURCE_DIR}/src/guiapi/include ${CMAKE_CURRENT_SOURCE_DIR}
  )

# tell build system about the test case
add_catch_test(rectangle_tests)
add_catch_test(gui_time_tests)
add_catch_test(gui_tests)

# std_text_input_layout_tests
add_executable(std_text_input_layout_tests ${CMAKE_CURRENT_SOURCE_DIR}/text_input_layout_tests.cpp)
target_include_directories(
  std_text_input_layout_tests
  PUBLIC ${CMAKE_SOURCE_DIR}/src/common ${CMAKE_SOURCE_DIR}/src/gui
         ${CMAKE_SOURCE_DIR}/src/gui/dialogs ${CMAKE_SOURCE_DIR}/include
  )
target_compile_definitions(std_text_input_layout_tests PUBLIC BOARD=BOARD_XBUDDY)
add_catch_test(std_text_input_layout_tests)

# mini_text_input_layout_tests
add_executable(mini_text_input_layout_tests ${CMAKE_CURRENT_SOURCE_DIR}/text_input_layout_tests.cpp)
target_include_directories(
  mini_text_input_layout_tests
  PUBLIC ${CMAKE_SOURCE_DIR}/src/common ${CMAKE_SOURCE_DIR}/src/gui
         ${CMAKE_SOURCE_DIR}/src/gui/dialogs ${CMAKE_SOURCE_DIR}/include
  )
target_compile_definitions(mini_text_input_layout_tests PUBLIC BOARD=BOARD_BUDDY)
add_catch_test(mini_text_input_layout_tests)

# color_tests
add_executable(
  color_tests
  ${CMAKE_CURRENT_SOURCE_DIR}/color_tests.cpp ${CMAKE_SOURCE_DIR}/src/common/utils/color.cpp
  ${CMAKE_SOURCE_DIR}/src/common/str_utils.cpp
  )
target_include_directories(
  color_tests PUBLIC ${CMAKE_SOURCE_DIR}/src/common ${CMAKE_SOURCE_DIR}/src/lang
                     ${CMAKE_SOURCE_DIR}/include
  )
add_catch_test(color_tests)

add_subdirectory(lazyfilelist)
add_subdirectory(jogwheel)
add_subdirectory(window)
add_subdirectory(neopixel)
