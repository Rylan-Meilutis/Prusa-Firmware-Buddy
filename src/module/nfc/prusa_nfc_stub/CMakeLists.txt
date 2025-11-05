# Stub OPTBackend implementation for OPTReader

add_library(prusa_nfc_stub)
target_sources(prusa_nfc_stub PRIVATE prusa_nfc_stub/ll_nfc_reader.cpp)
target_include_directories(prusa_nfc_stub PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(prusa_nfc_stub PUBLIC openprinttag SG14 buddy_utils raii)
