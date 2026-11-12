#include "io/relay_driver.hpp"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "io/i2c_bus.hpp"

namespace io::relay_driver {

namespace {
constexpr std::uint16_t kAddr = 0x20;
constexpr std::uint8_t kRegOutputPort = 0x01;
constexpr std::uint8_t kRegConfig = 0x03;
constexpr const char* kTag = "relay_driver";

i2c_master_dev_handle_t g_dev = nullptr;
std::uint8_t g_output_state = 0x00;

bool write_reg(std::uint8_t reg, std::uint8_t value) {
    if (g_dev == nullptr) {
        return false;
    }
    std::uint8_t payload[2] = {reg, value};
    esp_err_t err = i2c_master_transmit(g_dev, payload, sizeof(payload), pdMS_TO_TICKS(100));
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "I2C write reg 0x%02X failed: %s", reg, esp_err_to_name(err));
        return false;
    }
    return true;
}
}  // namespace

bool init() {
    i2c_master_bus_handle_t bus = io::i2c_bus::handle();
    if (bus == nullptr) {
        return false;
    }

    i2c_device_config_t cfg = {};
    cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    cfg.device_address = kAddr;
    cfg.scl_speed_hz = 100000;

    esp_err_t err = i2c_master_bus_add_device(bus, &cfg, &g_dev);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "i2c_master_bus_add_device failed: %s", esp_err_to_name(err));
        g_dev = nullptr;
        return false;
    }

    g_output_state = 0x00;
    bool ok = write_reg(kRegOutputPort, g_output_state);
    ok = write_reg(kRegConfig, 0x00) && ok;
    if (!ok) {
        ESP_LOGE(kTag, "Relay expander not responding on 0x%02X", kAddr);
    }
    return ok;
}

bool set_relay(std::uint8_t channel, bool on) {
    if (channel < 1 || channel > 8 || g_dev == nullptr) {
        return false;
    }
    const std::uint8_t bit = static_cast<std::uint8_t>(1u << (channel - 1));
    const std::uint8_t next = on ? static_cast<std::uint8_t>(g_output_state | bit)
                                 : static_cast<std::uint8_t>(g_output_state & ~bit);
    if (next == g_output_state) {
        return true;
    }
    if (!write_reg(kRegOutputPort, next)) {
        return false;
    }
    g_output_state = next;
    return true;
}

bool get_relay(std::uint8_t channel) {
    if (channel < 1 || channel > 8) {
        return false;
    }
    return (g_output_state & (1u << (channel - 1))) != 0;
}

}  // namespace io::relay_driver

