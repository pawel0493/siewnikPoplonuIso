#include <array>
#include <cinttypes>
#include <string>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "io/input_driver.hpp"
#include "io/md13s_pwm.hpp"
#include "io/relay_driver.hpp"
#include "isobus/ecu_identity.hpp"
#include "isobus/vt_app.hpp"
#include "nvs_flash.h"
#include "seeder_controller.hpp"
#include "storage/state_store.hpp"

namespace {
constexpr const char* kTag = "app_main";
constexpr TickType_t kLoopDelay = pdMS_TO_TICKS(20);
constexpr std::uint64_t kPersistDelayMs = 1500;

void apply_outputs(const siewnik::SeederSnapshot& s) {
    io::relay_driver::set_relay(1, s.relay_open_pulse);
    io::relay_driver::set_relay(2, s.relay_close_pulse);
    io::relay_driver::set_relay(3, s.relay_motor);
    io::relay_driver::set_relay(4, s.relay_mixer);
    io::relay_driver::set_relay(5, s.relay_led);
    io::md13s_pwm::set_percent(s.md13s_pin48_pwm_percent);
}

void set_default_aux_mapping(siewnik::SeederController& controller) {
    controller.set_aux_assignment(1, siewnik::AuxFunction::SectionOpenPulse);
    controller.set_aux_assignment(2, siewnik::AuxFunction::SectionClosePulse);
    controller.set_aux_assignment(3, siewnik::AuxFunction::MotorToggleManual);
    controller.set_aux_assignment(4, siewnik::AuxFunction::MixerToggleManual);
    controller.set_aux_assignment(5, siewnik::AuxFunction::LedToggleManual);
}
}  // namespace

extern "C" void app_main(void) {
    esp_err_t nvs_result = nvs_flash_init();
    if (nvs_result == ESP_ERR_NVS_NO_FREE_PAGES || nvs_result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_result = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_result);

    const bool relay_ok = io::relay_driver::init();
    const bool pwm_ok = io::md13s_pwm::init();
    io::input_driver::init();
    ESP_LOGI(kTag, "drivers: relay=%s pwm=%s", relay_ok ? "OK" : "FAIL", pwm_ok ? "OK" : "FAIL");

    siewnik::SeederController controller(500);
    std::string loaded;
    bool restored = false;
    if (storage::state_store::load(loaded)) {
        if (controller.import_persistent_state(loaded)) {
            ESP_LOGI(kTag, "Restored controller state from NVS");
            restored = true;
        } else {
            ESP_LOGW(kTag, "NVS state invalid, using defaults");
        }
    }
    if (!restored) {
        set_default_aux_mapping(controller);
    }

    // Let the debouncer settle on the real pin levels first, so a button held at power-up
    // is treated as already pressed and does not fire its function.
    for (int i = 0; i < io::input_driver::kDebounceSamples; ++i) {
        io::input_driver::update();
        vTaskDelay(kLoopDelay);
    }
    std::array<bool, 8> last_di{};
    for (std::uint8_t ch = 1; ch <= 8; ++ch) {
        last_di[ch - 1] = io::input_driver::read_debounced(ch);
        controller.prime_aux_button_state(ch, last_di[ch - 1]);
    }

    std::uint64_t last_log_ms = 0;
    std::uint64_t last_vt_publish_ms = 0;
    std::uint64_t dirty_since_ms = 0;
    std::string last_saved_state = controller.export_persistent_state();
    storage::state_store::save(last_saved_state);

    auto internal_ecu = iso::ecu_identity::init();
    iso::vt_app::init(internal_ecu);

    while (true) {
        const std::uint64_t now_ms = static_cast<std::uint64_t>(esp_timer_get_time() / 1000);
        controller.set_now_ms(static_cast<std::uint32_t>(now_ms));
        io::input_driver::update();
        iso::vt_app::apply_pending_commands(controller);

        for (std::uint8_t ch = 1; ch <= 8; ++ch) {
            const bool current = io::input_driver::read_debounced(ch);
            controller.handle_aux_button_event(ch, current);
            if (!last_di[ch - 1] && current) {
                if (ch == 6) {
                    controller.set_md13s_pwm_percent(controller.snapshot().md13s_pwm_percent + 5);
                } else if (ch == 7) {
                    controller.set_md13s_pwm_percent(controller.snapshot().md13s_pwm_percent - 5);
                }
            }
            last_di[ch - 1] = current;
        }

        const auto snap = controller.snapshot();
        apply_outputs(snap);
        if ((now_ms - last_vt_publish_ms) >= 200) {
            last_vt_publish_ms = now_ms;
            iso::vt_app::publish_snapshot(snap);
        }

        const std::string state_blob = controller.export_persistent_state();
        if (state_blob != last_saved_state) {
            if (dirty_since_ms == 0) {
                dirty_since_ms = now_ms;
            } else if ((now_ms - dirty_since_ms) >= kPersistDelayMs) {
                if (storage::state_store::save(state_blob)) {
                    last_saved_state = state_blob;
                    dirty_since_ms = 0;
                }
            }
        } else {
            dirty_since_ms = 0;
        }

        if ((now_ms - last_log_ms) >= 1000) {
            last_log_ms = now_ms;
            ESP_LOGI(
                kTag,
                "sec=%s R1=%d R2=%d R3=%d R4=%d R5=%d pwm48=%d%% DI1..8=%d%d%d%d%d%d%d%d",
                snap.section_is_open ? "OPEN" : "CLOSED",
                snap.relay_open_pulse,
                snap.relay_close_pulse,
                snap.relay_motor,
                snap.relay_mixer,
                snap.relay_led,
                snap.md13s_pin48_pwm_percent,
                static_cast<int>(last_di[0]),
                static_cast<int>(last_di[1]),
                static_cast<int>(last_di[2]),
                static_cast<int>(last_di[3]),
                static_cast<int>(last_di[4]),
                static_cast<int>(last_di[5]),
                static_cast<int>(last_di[6]),
                static_cast<int>(last_di[7]));
            ESP_LOGI(kTag, "VT: partner=%s connected=%s",
                     iso::vt_app::is_partner_claimed() ? "YES" : "NO",
                     iso::vt_app::is_connected() ? "YES" : "NO");
        }

        vTaskDelay(kLoopDelay);
    }
}

