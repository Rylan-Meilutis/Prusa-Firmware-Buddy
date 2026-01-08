set(OPT_SRC "${CMAKE_SOURCE_DIR}/src/common/feature/openprinttag")

# buddy_openprinttag_multirequest_tests
add_executable(
  buddy_openprinttag_multirequest_tests
  ${OPT_SRC}/tool_tag.cpp ${OPT_SRC}/detail/requests_base.cpp
  ${OPT_SRC}/detail/requests_multi_base.cpp buddy_openprinttag_multirequest_tests.cpp
  )
target_include_directories(
  buddy_openprinttag_multirequest_tests PRIVATE stub_include_multirequest
                                                ${CMAKE_SOURCE_DIR}/src/common
  )
target_link_libraries(
  buddy_openprinttag_multirequest_tests openprinttag_fields buddy_utils logging-mock
  )
add_catch_test(buddy_openprinttag_multirequest_tests)

# buddy_openprinttag_datautils_tests
add_executable(
  buddy_openprinttag_datautils_tests
  ${OPT_SRC}/data_utils.cpp
  ${OPT_SRC}/detail/requests_base.cpp
  ${OPT_SRC}/detail/requests_multi_base.cpp
  buddy_openprinttag_datautils_tests.cpp
  ${CMAKE_SOURCE_DIR}/src/common/filament_presets.cpp
  ${CMAKE_SOURCE_DIR}/src/common/filament_common.cpp
  ${CMAKE_SOURCE_DIR}/src/common/filament_list.cpp
  )
target_include_directories(
  buddy_openprinttag_datautils_tests PRIVATE stub_include_datautils ${CMAKE_SOURCE_DIR}/include
                                             ${CMAKE_SOURCE_DIR}/src/common
  )
target_link_libraries(
  buddy_openprinttag_datautils_tests openprinttag_fields buddy_utils logging-mock
  )
add_catch_test(buddy_openprinttag_datautils_tests)
