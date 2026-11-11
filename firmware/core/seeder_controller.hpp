#pragma once

#include <cstdint>

namespace siewnik {

enum class SectionState : std::uint8_t {
    Closed,
    OpeningPulse,
    Open,
    ClosingPulse,
};

struct SeederSnapshot {
    bool relay_open_pulse = false;    // R1
    bool relay_close_pulse = false;   // R2
    bool relay_motor = false;         // R3
    bool relay_mixer = false;         // R4
    bool relay_led = false;           // R5

    bool section_is_open = false;
    bool sowing_active = false;
    SectionState section_state = SectionState::Closed;

    bool motor_auto_with_section = false;
    bool mixer_auto_with_section = false;

    int md13s_setpoint_percent = 0;   // 0..100
    int md13s_effective_percent = 0;  // 0..100, 0 when motor relay is off
};

class SeederController {
public:
    explicit SeederController(std::uint32_t pulse_ms = 500);

    void set_now_ms(std::uint32_t now_ms);

    void request_section_open();
    void request_section_close();

    void set_motor_manual(bool enabled);
    void set_mixer_manual(bool enabled);
    void set_led_manual(bool enabled);

    void set_motor_auto_with_section(bool enabled);
    void set_mixer_auto_with_section(bool enabled);

    void set_md13s_setpoint_percent(int percent_0_100);

    SeederSnapshot snapshot() const;

private:
    static int clamp_percent(int value);

    std::uint32_t pulse_duration_ms_;
    std::uint32_t now_ms_ = 0;
    std::uint32_t pulse_deadline_ms_ = 0;

    SectionState section_state_ = SectionState::Closed;
    bool section_is_open_ = false;

    bool motor_manual_ = false;
    bool mixer_manual_ = false;
    bool led_manual_ = false;

    bool motor_auto_with_section_ = false;
    bool mixer_auto_with_section_ = false;

    int md13s_setpoint_percent_ = 0;
};

}  // namespace siewnik

