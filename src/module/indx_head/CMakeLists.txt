add_library(indx_head INTERFACE)
target_include_directories(indx_head INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(indx_head INTERFACE buddy_utils)
