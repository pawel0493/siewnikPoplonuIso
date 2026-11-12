idf_component_register(
    SRCS
        "app_main.cpp"
        "io/i2c_bus.cpp"
        "io/relay_driver.cpp"
        "io/input_driver.cpp"
        "io/md13s_pwm.cpp"
        "storage/state_store.cpp"
        "isobus/ecu_identity.cpp"
        "isobus/vt_app.cpp"
        "../core/seeder_controller.cpp"
    INCLUDE_DIRS
        "."
        "../core"
    REQUIRES
        nvs_flash
        AgIsoStack-plus-plus
    PRIV_REQUIRES
        driver
        esp_timer
    EMBED_FILES
        "isobus/object_pool.iop"
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)

