target_sources(Marlin PRIVATE marlin_temptable.cpp steady_state_hotend.cpp thermal_runaway.cpp)

add_subdirectory(hotend_regulator)
