idf_component_register(REQUIRES driver)

set(CAN_DRIVER "TWAI")

if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/upstream/CMakeLists.txt")
    message(
        FATAL_ERROR
        "Brakuje submodułu AgIsoStack-plus-plus/upstream. "
        "Uruchom: git submodule update --init --recursive"
    )
endif()

add_subdirectory(upstream agisostack_upstream)

target_link_libraries(${COMPONENT_LIB} INTERFACE Isobus HardwareIntegration Utility)

