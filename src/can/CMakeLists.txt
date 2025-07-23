target_include_directories(firmware PRIVATE .)

target_sources(
  firmware PRIVATE can_driver_fdcan.cpp cyphal_proto_sender.cpp cyphal_proto_suber.cpp
                   cyphal_simple_register.cpp cyphal_task.cpp cyphal_timesync.cpp
  )

if(HAS_CYPHAL_LOGGING)
  target_sources(firmware PRIVATE cyphal_record.cpp)
endif()

if(CYPHAL_CAN_STATS)
  target_sources(firmware PRIVATE cyphal_can_stats.cpp)
endif()
