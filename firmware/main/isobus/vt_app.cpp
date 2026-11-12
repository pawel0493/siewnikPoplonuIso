#include "isobus/vt_app.hpp"

#include <atomic>
#include <cstddef>
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
constexpr std::uint32_t kPoolDataMaskWidthPx = 480;
constexpr std::uint32_t kPoolSoftKeyWidthPx = 60;
constexpr std::size_t kStatusLineLength = 20;  // must match the pool generator
constexpr std::size_t kStatusLineCount = 5;

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
std::string g_sent_status[kStatusLineCount];  // last text sent per status line, to avoid repeats

void queue_command(PendingCommand command) {
    g_pending_commands.fetch_or(static_cast<std::uint32_t>(command), std::memory_order_relaxed);
}

void handle_soft_key_event(const isobus::VirtualTerminalClient::VTKeyEvent& event) {
    if (event.keyEvent != isobus::VirtualTerminalClient::KeyActivationCode::ButtonUnlatchedOrReleased) {
        return;
    }

    switch (event.objectID) {
        case object_pool_ids::kKeyOpen: queue_command(kCmdOpen); break;
        case object_pool_ids::kKeyClose: queue_command(kCmdClose); break;
        case object_pool_ids::kKeyMotor: queue_command(kCmdMotorToggle); break;
        case object_pool_ids::kKeyMixer: queue_command(kCmdMixerToggle); break;
        case object_pool_ids::kKeyLed: queue_command(kCmdLedToggle); break;
        case object_pool_ids::kKeyPwmUp: queue_command(kCmdPwmUp); break;
        case object_pool_ids::kKeyPwmDown: queue_command(kCmdPwmDown); break;
        case object_pool_ids::kKeyAutoMotor: queue_command(kCmdAutoMotorToggle); break;
        case object_pool_ids::kKeyAutoMixer: queue_command(kCmdAutoMixerToggle); break;
        default: break;
    }
}

struct AuxFunctionBinding {
    std::uint16_t functionObjectID;
    PendingCommand command;
};

constexpr AuxFunctionBinding kAuxBindings[] = {
    {object_pool_ids::kAuxOpen, kCmdOpen},
    {object_pool_ids::kAuxClose, kCmdClose},
    {object_pool_ids::kAuxMotorToggle, kCmdMotorToggle},
    {object_pool_ids::kAuxMixerToggle, kCmdMixerToggle},
    {object_pool_ids::kAuxLedToggle, kCmdLedToggle},
};
constexpr std::size_t kAuxBindingCount = sizeof(kAuxBindings) / sizeof(kAuxBindings[0]);
bool g_aux_pressed[kAuxBindingCount] = {};

// The VT reports the joystick state on every change, so act only on the press edge.
void handle_aux_function_event(const isobus::VirtualTerminalClient::AuxiliaryFunctionEvent& event) {
    for (std::size_t i = 0; i < kAuxBindingCount; ++i) {
        if (kAuxBindings[i].functionObjectID != event.function.functionObjectID) {
            continue;
        }
        const bool pressed = event.value1 != 0;
        if (pressed && !g_aux_pressed[i]) {
            queue_command(kAuxBindings[i].command);
        }
        g_aux_pressed[i] = pressed;
        return;
    }
}

// VT strings keep the length they have in the pool, so every status line is padded to it.
std::string status_text(const std::string& text) {
    std::string line = text;
    line.resize(kStatusLineLength, ' ');
    return line;
}

const char* section_text(siewnik::SectionState state) {
    switch (state) {
        case siewnik::SectionState::Closed: return "ZAMKNIETA";
        case siewnik::SectionState::OpeningPulse: return "OTWIERANIE";
        case siewnik::SectionState::Open: return "OTWARTA";
        case siewnik::SectionState::ClosingPulse: return "ZAMYKANIE";
    }
    return "";
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
    g_vt_client->get_auxiliary_function_event_dispatcher().add_listener(handle_aux_function_event);
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
    if (commands & kCmdMotorToggle) controller.toggle_motor_manual();
    if (commands & kCmdMixerToggle) controller.toggle_mixer_manual();
    if (commands & kCmdLedToggle) controller.toggle_led_manual();
    if (commands & kCmdPwmUp) controller.set_md13s_pwm_percent(controller.snapshot().md13s_pwm_percent + 5);
    if (commands & kCmdPwmDown) controller.set_md13s_pwm_percent(controller.snapshot().md13s_pwm_percent - 5);
    if (commands & kCmdAutoMotorToggle) controller.set_motor_auto_with_section(!controller.snapshot().motor_auto_with_section);
    if (commands & kCmdAutoMixerToggle) controller.set_mixer_auto_with_section(!controller.snapshot().mixer_auto_with_section);
}

void publish_snapshot(const siewnik::SeederSnapshot& snapshot) {
    if (!g_vt_client || !g_vt_client->get_is_connected()) {
        // A new pool upload resets the VT strings, so forget what was sent before.
        for (std::string& sent : g_sent_status) {
            sent.clear();
        }
        return;
    }

    const std::string lines[kStatusLineCount] = {
        status_text(std::string("SEKCJA: ") + section_text(snapshot.section_state)),
        status_text(snapshot.relay_motor ? "SILNIK: ON" : "SILNIK: OFF"),
        status_text(snapshot.relay_mixer ? "MIESZADLO: ON" : "MIESZADLO: OFF"),
        status_text(snapshot.relay_led ? "LED: ON" : "LED: OFF"),
        status_text("PWM: " + std::to_string(snapshot.md13s_pwm_percent) + "%"),
    };
    const std::uint16_t ids[kStatusLineCount] = {
        object_pool_ids::kTextSection,
        object_pool_ids::kTextMotor,
        object_pool_ids::kTextMixer,
        object_pool_ids::kTextLed,
        object_pool_ids::kTextPwm,
    };

    for (std::size_t i = 0; i < kStatusLineCount; ++i) {
        if (lines[i] == g_sent_status[i]) {
            continue;
        }
        if (g_vt_client->send_change_string_value(ids[i], lines[i])) {
            g_sent_status[i] = lines[i];
        }
    }
}

}  // namespace iso::vt_app
