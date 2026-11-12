idf_component_register(REQUIRES driver)

set(CAN_DRIVER "TWAI")

add_subdirectory(upstream agisostack_upstream)

target_link_libraries(${COMPONENT_LIB} INTERFACE Isobus HardwareIntegration Utility)

