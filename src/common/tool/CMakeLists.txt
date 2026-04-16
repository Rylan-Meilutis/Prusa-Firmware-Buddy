add_subdirectory(hotend)

target_sources(firmware PRIVATE physical_tool.cpp)

if(${BOARD} STREQUAL "BUDDY")
  target_sources(
    firmware PRIVATE hotend/hotend/local_hotend.cpp printer/tools_buddy.cpp
                     tool/standard_fff_physical_tool.cpp
    )

elseif(${BOARD} STREQUAL "XBUDDY")
  target_sources(firmware PRIVATE tool/standard_fff_physical_tool.cpp)
  if(HAS_INDX)
    target_sources(
      firmware PRIVATE hotend/hotend/dummy_hotend.cpp hotend/hotend/indx_hotend.cpp
                       printer/tools_indx.cpp tool/dummy_physical_tool.cpp
      )
  else()
    target_sources(firmware PRIVATE hotend/hotend/local_hotend.cpp printer/tools_xbuddy.cpp)
  endif()

elseif(${BOARD} STREQUAL "XLBUDDY")
  target_sources(
    firmware
    PRIVATE hotend/hotend/dummy_hotend.cpp hotend/hotend/dwarf_hotend.cpp printer/tools_xlbuddy.cpp
            tool/dummy_physical_tool.cpp tool/standard_fff_physical_tool.cpp
    )

elseif(${BOARD} STREQUAL "DWARF")
  target_sources(
    firmware PRIVATE hotend/hotend/local_hotend.cpp printer/tools_dwarf.cpp
                     tool/standard_fff_physical_tool.cpp
    )

else()
  message(FATAL_ERROR Unknown tool configuration)
endif()
