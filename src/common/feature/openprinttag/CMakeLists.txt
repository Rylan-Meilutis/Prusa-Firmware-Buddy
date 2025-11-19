target_sources(firmware PRIVATE requests_base.cpp requests_read.cpp tool_tag.cpp)

# Will get optimized out, added just for checking that it compiles
target_sources(firmware PRIVATE usage_example.cpp)

target_link_libraries(firmware PUBLIC openprinttag openprinttag_fields)
