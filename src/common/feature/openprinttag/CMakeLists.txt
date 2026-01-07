add_subdirectory(detail)
add_subdirectory(filament_usage_tracker)

target_sources(firmware PRIVATE data_utils.cpp requests_read.cpp tool_tag.cpp tool_tag_buddy.cpp)

# Will get optimized out, added just for checking that it compiles
target_sources(firmware PRIVATE usage_example.cpp)

target_link_libraries(firmware PUBLIC openprinttag openprinttag_fields)
