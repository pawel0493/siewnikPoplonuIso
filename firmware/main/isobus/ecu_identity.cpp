#include "isobus/ecu_identity.hpp"

#include <cinttypes>
#include <string>

#include "driver/gpio.h"
#include "driver/twai.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "isobus/hardware_integration/can_hardware_interface.hpp"
#include "isobus/hardware_integration/twai_plugin.hpp"
#include "isobus/isobus/can_NAME.hpp"
#include "isobus/isobus/can_network_manager.hpp"
#include "isobus/isobus/can_stack_logger.hpp"

namespace iso::ecu_identity {

namespace {
constexpr gpio_num_t kTxPin = GPIO_NUM_17;
constexpr gpio_num_t kRxPin = GPIO_NUM_18;
constexpr std::uint16_t kManufacturerCode = 1407;
constexpr const char* kTag = "ecu_identity";

class EspLogStackLogger : public isobus::CANStackLogger {
public:
    void sink_CAN_stack_log(LoggingLevel level, const std::string& text) override {
        switch (level) {
            case LoggingLevel::Debug: ESP_LOGD(kTag, "%s", text.c_str()); break;
            case LoggingLevel::Info: ESP_LOGI(kTag, "%s", text.c_str()); break;
            case LoggingLevel::Warning: ESP_LOGW(kTag, "%s", text.c_str()); break;
            case LoggingLevel::Error:
            case LoggingLevel::Critical:
            default: ESP_LOGE(kTag, "%s", text.c_str()); break;
        }
    }
};

EspLogStackLogger g_stack_logger;
}  // namespace

std::shared_ptr<isobus::InternalControlFunction> init() {
    isobus::CANStackLogger::set_can_stack_logger_sink(&g_stack_logger);
    isobus::CANStackLogger::set_log_level(isobus::CANStackLogger::LoggingLevel::Info);

    twai_general_config_t twai_config = TWAI_GENERAL_CONFIG_DEFAULT(kTxPin, kRxPin, TWAI_MODE_NORMAL);
    twai_timing_config_t twai_timing = TWAI_TIMING_CONFIG_250KBITS();
    twai_filter_config_t twai_filter = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    auto can_driver = std::make_shared<isobus::TWAIPlugin>(&twai_config, &twai_timing, &twai_filter);

    isobus::CANHardwareInterface::set_number_of_can_channels(1);
    isobus::CANHardwareInterface::assign_can_channel_frame_handler(0, can_driver);

    if (!isobus::CANHardwareInterface::start() || !can_driver->get_is_valid()) {
        ESP_LOGE(kTag, "Failed to start TWAI/CAN stack");
        return nullptr;
    }

    isobus::NAME name(0);
    name.set_arbitrary_address_capable(true);
    name.set_industry_group(static_cast<std::uint8_t>(isobus::NAME::IndustryGroup::AgriculturalAndForestryEquipment));
    name.set_device_class(static_cast<std::uint8_t>(isobus::NAME::DeviceClass::NonSpecific));
    name.set_function_code(static_cast<std::uint8_t>(isobus::NAME::Function::IOController));
    name.set_ecu_instance(0);
    name.set_function_instance(0);
    name.set_device_class_instance(0);
    name.set_manufacturer_code(kManufacturerCode);

    std::uint8_t mac[6] = {};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    std::uint32_t identity = ((static_cast<std::uint32_t>(mac[3]) << 16) |
                              (static_cast<std::uint32_t>(mac[4]) << 8) |
                              static_cast<std::uint32_t>(mac[5])) &
                             0x1FFFFF;
    name.set_identity_number(identity);

    std::uint8_t preferred_address = static_cast<std::uint8_t>(128 + (identity % 100));
    auto internal_ecu = isobus::CANNetworkManager::CANNetwork.create_internal_control_function(name, 0, preferred_address);
    ESP_LOGI(kTag, "Claiming address, preferred=%u identity=0x%06" PRIX32, preferred_address, identity);
    return internal_ecu;
}

}  // namespace iso::ecu_identity

