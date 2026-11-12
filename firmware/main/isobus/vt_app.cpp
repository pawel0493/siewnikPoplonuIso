#include "isobus/vt_app.hpp"

#include <atomic>
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

#include "esp_log.h"
#include "isobus/isobus/can_NAME.hpp"
#include "isobus/isobus/can_network_manager.hpp"
#include "isobus/isobus/can_partnered_control_function.hpp"
#include "isobus/isobus/isobus_virtual_terminal_client.hpp"
#include "isobus/object_pool_ids.hpp"
#include "isobus/utility/iop_file_interface.hpp"
#include "seeder_controller.hpp"

extern "C" const std::uint8_t object_pool_iop_start[] asm("_binary_object_pool_iop_start");
extern "C" const std::uint8_t object_pool_iop_end[] asm("_binary_object_pool_iop_end");

namespace iso::vt_app {

namespace {
constexpr const char* kTag = "vt_app";
constexpr std::uint8_t kColourBlack = 0;
constexpr std::uint32_t kPoolDataMaskWidthPx = 480;
constexpr std::uint32_t kPoolSoftKeyWidthPx = 60;

enum PendingCommand : std::uint32_t {
    kCmdOpen = 1u << 0,
    kCmdClose = 1u << 1,
    kCmdMotorToggle = 1u << 2,
    kCmdMixerToggle = 1u << 3,
    kCmdLedToggle = 1u << 4,
    kCmdPwmUp = 1u << 5,
    kCmdPwmDown = 1u << 6,
    kCmdAutoMotorToggle = 1u << 7,
    kCmdAutoMixerToggle = 1u << 8,
};

std::shared_ptr<isobus::VirtualTerminalClient> g_vt_client;
std::shared_ptr<isobus::PartneredControlFunction> g_vt_partner;
std::atomic<std::uint32_t> g_pending_commands{0};

void queue_command(PendingCommand command) {
    g_pending_commands.fetch_or(static_cast<std::uint32_t>(command), std::memory_order_relaxed);
}

void handle_soft_key_event(const isobus::VirtualTerminalClient::VTKeyEvent& event) {
    if (event.keyEvent != isobus::VirtualTerminalClient::KeyActivationCode::ButtonUnlatchedOrReleased) {
        return;
    }

    switch (event.objectID) {
        case 1211: queue_command(kCmdOpen); break;         // SK1
        case 1212: queue_command(kCmdClose); break;        // SK2
        case 1213: queue_command(kCmdMotorToggle); break;  // SK3
        case 1214: queue_command(kCmdMixerToggle); break;  // SK4
        case 1215: queue_command(kCmdLedToggle); break;    // SK5
        case 1216: queue_command(kCmdPwmUp); break;        // SK6
        case 1217: queue_command(kCmdPwmDown); break;      // SK7
        case 1218: queue_command(kCmdAutoMotorToggle); break;  // SK8
        case 1219: queue_command(kCmdAutoMixerToggle); break;  // SK9
        default: break;
    }
}
}  // namespace

void init(std::shared_ptr<isobus::InternalControlFunction> internal_ecu) {
    if (!internal_ecu) {
        ESP_LOGE(kTag, "No ECU, VT not started");
        return;
    }

    const isobus::NAMEFilter vt_function_filter(
        isobus::NAME::NAMEParameters::FunctionCode,
        static_cast<std::uint8_t>(isobus::NAME::Function::VirtualTerminal));
    const std::vector<isobus::NAMEFilter> vt_filters = {vt_function_filter};
    g_vt_partner = isobus::CANNetworkManager::CANNetwork.create_partnered_control_function(0, vt_filters);

    const std::uint32_t pool_size = static_cast<std::uint32_t>(object_pool_iop_end - object_pool_iop_start);
    const std::string pool_version = isobus::IOPFileInterface::hash_object_pool_to_version(object_pool_iop_start, pool_size);

    g_vt_client = std::make_shared<isobus::VirtualTerminalClient>(g_vt_partner, internal_ecu);
    g_vt_client->set_object_pool(0, object_pool_iop_start, pool_size, pool_version);
    g_vt_client->set_object_pool_scaling(0, kPoolDataMaskWidthPx, kPoolSoftKeyWidthPx);
    g_vt_client->get_vt_soft_key_event_dispatcher().add_listener(handle_soft_key_event);
    g_vt_client->initialize(true);
    ESP_LOGI(kTag, "VT client initialized");
}

bool is_connected() {
    return g_vt_client && g_vt_client->get_is_connected();
}

bool is_partner_claimed() {
    return g_vt_partner && g_vt_partner->get_address_valid();
}

void apply_pending_commands(siewnik::SeederController& controller) {
    const std::uint32_t commands = g_pending_commands.exchange(0, std::memory_order_relaxed);
    if (commands == 0) {
        return;
    }

    if (commands & kCmdOpen) controller.request_section_open();
    if (commands & kCmdClose) controller.request_section_close();
    if (commands & kCmdMotorToggle) controller.set_motor_manual(!controller.snapshot().relay_motor);
    if (commands & kCmdMixerToggle) controller.set_mixer_manual(!controller.snapshot().relay_mixer);
    if (commands & kCmdLedToggle) controller.set_led_manual(!controller.snapshot().relay_led);
    if (commands & kCmdPwmUp) controller.set_md13s_pwm_percent(controller.snapshot().md13s_pwm_percent + 5);
    if (commands & kCmdPwmDown) controller.set_md13s_pwm_percent(controller.snapshot().md13s_pwm_percent - 5);
    if (commands & kCmdAutoMotorToggle) controller.set_motor_auto_with_section(!controller.snapshot().motor_auto_with_section);
    if (commands & kCmdAutoMixerToggle) controller.set_mixer_auto_with_section(!controller.snapshot().mixer_auto_with_section);
}

void publish_snapshot(const siewnik::SeederSnapshot& snapshot) {
    if (!g_vt_client || !g_vt_client->get_is_connected()) {
        return;
    }

    for (int channel = 1; channel <= 5; ++channel) {
        bool on = false;
        switch (channel) {
            case 1: on = snapshot.relay_open_pulse; break;
            case 2: on = snapshot.relay_close_pulse; break;
            case 3: on = snapshot.relay_motor; break;
            case 4: on = snapshot.relay_mixer; break;
            case 5: on = snapshot.relay_led; break;
        }
        g_vt_client->send_change_fill_attributes(
            object_pool_ids::relay_fill_attr_id(channel),
            on ? isobus::VirtualTerminalClient::FillType::FillWithSpecifiedColourInFillColourAttribute
               : isobus::VirtualTerminalClient::FillType::NoFill,
            kColourBlack,
            isobus::NULL_OBJECT_ID);
    }

    char title[44] = {};
    std::snprintf(title,
                  sizeof(title),
                  "Siewnik S1:%s PWM:%d%%",
                  snapshot.section_is_open ? "OPEN" : "CLOSED",
                  snapshot.md13s_pin48_pwm_percent);
    g_vt_client->send_change_string_value(object_pool_ids::kTitleString, title);
}

}  // namespace iso::vt_app

