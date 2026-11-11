#include "seeder_controller.hpp"

namespace siewnik {

SeederController::SeederController(std::uint32_t pulse_ms)
    : open_pulse_duration_ms_(pulse_ms), close_pulse_duration_ms_(pulse_ms) {}

int SeederController::clamp_percent(int value) {
    if (value < 0) {
        return 0;
    }
    if (value > 100) {
        return 100;
    }
    return value;
}

void SeederController::set_now_ms(std::uint32_t now_ms) {
    now_ms_ = now_ms;

    if (section_state_ == SectionState::OpeningPulse && now_ms_ >= pulse_deadline_ms_) {
        section_state_ = SectionState::Open;
        section_is_open_ = true;
    } else if (section_state_ == SectionState::ClosingPulse && now_ms_ >= pulse_deadline_ms_) {
        section_state_ = SectionState::Closed;
        section_is_open_ = false;
    }
}

void SeederController::request_section_open() {
    section_state_ = SectionState::OpeningPulse;
    pulse_deadline_ms_ = now_ms_ + open_pulse_duration_ms_;
}

void SeederController::request_section_close() {
    section_state_ = SectionState::ClosingPulse;
    pulse_deadline_ms_ = now_ms_ + close_pulse_duration_ms_;
}

void SeederController::set_motor_manual(bool enabled) { motor_manual_ = enabled; }
void SeederController::set_mixer_manual(bool enabled) { mixer_manual_ = enabled; }
void SeederController::set_led_manual(bool enabled) { led_manual_ = enabled; }

void SeederController::set_motor_auto_with_section(bool enabled) { motor_auto_with_section_ = enabled; }
void SeederController::set_mixer_auto_with_section(bool enabled) { mixer_auto_with_section_ = enabled; }

void SeederController::set_open_pulse_ms(std::uint32_t pulse_ms) { open_pulse_duration_ms_ = pulse_ms; }
void SeederController::set_close_pulse_ms(std::uint32_t pulse_ms) { close_pulse_duration_ms_ = pulse_ms; }

void SeederController::set_md13s_pwm_limit_percent(int percent_0_100) {
    md13s_pwm_limit_percent_ = clamp_percent(percent_0_100);
}

void SeederController::set_md13s_setpoint_percent(int percent_0_100) {
    md13s_setpoint_percent_ = clamp_percent(percent_0_100);
}

SeederSnapshot SeederController::snapshot() const {
    SeederSnapshot out;

    out.relay_open_pulse = (section_state_ == SectionState::OpeningPulse);
    out.relay_close_pulse = (section_state_ == SectionState::ClosingPulse);
    out.section_state = section_state_;
    out.section_is_open = section_is_open_;
    out.sowing_active = section_is_open_;

    out.motor_auto_with_section = motor_auto_with_section_;
    out.mixer_auto_with_section = mixer_auto_with_section_;
    out.open_pulse_ms = open_pulse_duration_ms_;
    out.close_pulse_ms = close_pulse_duration_ms_;

    out.relay_motor = motor_auto_with_section_ ? section_is_open_ : motor_manual_;
    out.relay_mixer = mixer_auto_with_section_ ? section_is_open_ : mixer_manual_;
    out.relay_led = led_manual_;

    out.md13s_pwm_limit_percent = md13s_pwm_limit_percent_;
    out.md13s_setpoint_percent = md13s_setpoint_percent_;
    out.md13s_effective_percent = out.relay_motor
                                      ? (md13s_setpoint_percent_ * md13s_pwm_limit_percent_) / 100
                                      : 0;

    return out;
}

}  // namespace siewnik

