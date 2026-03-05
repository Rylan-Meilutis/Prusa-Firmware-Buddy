target_sources(firmware PRIVATE frame_gcode_incompatible.cpp)

if(HAS_MMU2 OR HAS_TOOLCHANGER)
  target_sources(firmware PRIVATE frame_tool_mapping.cpp)
endif()
