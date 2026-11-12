#include "io/i2c_bus.hpp"

#include "driver/gpio.h"
#include "esp_log.h"

namespace io::i2c_bus {

namespace {
constexpr gpio_num_t kSdaPin = GPIO_NUM_42;
constexpr gpio_num_t kSclPin = GPIO_NUM_41;
constexpr i2c_port_num_t kPort = I2C_NUM_0;
constexpr const char* kTag = "i2c_bus";

i2c_master_bus_handle_t g_bus = nullptr;
}  // namespace

i2c_master_bus_handle_t handle() {
    if (g_bus != nullptr) {
        return g_bus;
    }

    i2c_master_bus_config_t cfg = {};
    cfg.i2c_port = kPort;
    cfg.sda_io_num = kSdaPin;
    cfg.scl_io_num = kSclPin;
    cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    cfg.glitch_ignore_cnt = 7;
    cfg.flags.enable_internal_pullup = true;

    esp_err_t err = i2c_new_master_bus(&cfg, &g_bus);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "i2c_new_master_bus failed: %s", esp_err_to_name(err));
        g_bus = nullptr;
    }
    return g_bus;
}

}  // namespace io::i2c_bus

