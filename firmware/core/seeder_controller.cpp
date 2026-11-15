#include "seeder_controller.hpp"

#include <charconv>
#include <sstream>
#include <string>

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

const char* SeederController::aux_function_to_token(AuxFunction function) {
    switch (function) {
        case AuxFunction::None: return "NONE";
        case AuxFunction::SectionOpenPulse: return "OPEN";
        case AuxFunction::SectionClosePulse: return "CLOSE";
        case AuxFunction::SectionToggle: return "TOGGLE";
        case AuxFunction::MotorToggleManual: return "MOTOR_TOGGLE";
        case AuxFunction::MixerToggleManual: return "MIXER_TOGGLE";
        case AuxFunction::LedToggleManual: return "LED_TOGGLE";
    }
    return "NONE";
}

bool SeederController::aux_function_from_token(const std::string& token, AuxFunction& out) {
    if (token == "NONE") out = AuxFunction::None;
    else if (token == "OPEN") out = AuxFunction::SectionOpenPulse;
    else if (token == "CLOSE") out = AuxFunction::SectionClosePulse;
    else if (token == "TOGGLE") out = AuxFunction::SectionToggle;
    else if (token == "MOTOR_TOGGLE") out = AuxFunction::MotorToggleManual;
    else if (token == "MIXER_TOGGLE") out = AuxFunction::MixerToggleManual;
    else if (token == "LED_TOGGLE") out = AuxFunction::LedToggleManual;
    else return false;
    return true;
}

bool SeederController::parse_bool_int(const std::string& value, bool& out) {
    if (value == "0") {
        out = false;
        return true;
    }
    if (value == "1") {
        out = true;
        return true;
    }
    return false;
}

bool SeederController::parse_u32(const std::string& value, std::uint32_t& out) {
    if (value.empty()) {
        return false;
    }
    std::uint32_t parsed = 0;
    const char* first = value.data();
    const char* last = value.data() + value.size();
    auto result = std::from_chars(first, last, parsed);
    if (result.ec != std::errc() || result.ptr != last) {
        return false;
    }
    out = parsed;
    return true;
}

bool SeederController::parse_int(const std::string& value, int& out) {
    if (value.empty()) {
        return false;
    }
    int parsed = 0;
    const char* first = value.data();
    const char* last = value.data() + value.size();
    auto result = std::from_chars(first, last, parsed);
    if (result.ec != std::errc() || result.ptr != last) {
        return false;
    }
    out = parsed;
    return true;
}

bool SeederController::valid_button(std::uint8_t button) const {
    return button >= 1 && button <= kMaxAuxButtons;
}

void SeederController::apply_aux_function(AuxFunction function) {
    switch (function) {
        case AuxFunction::None:
            break;
        case AuxFunction::SectionOpenPulse:
            request_section_open();
            break;
        case AuxFunction::SectionClosePulse:
            request_section_close();
            break;
        case AuxFunction::SectionToggle:
            if (section_state_ == SectionState::Open || section_state_ == SectionState::OpeningPulse) {
                request_section_close();
            } else {
                request_section_open();
            }
            break;
        case AuxFunction::MotorToggleManual:
            toggle_motor_manual();
            break;
        case AuxFunction::MixerToggleManual:
            toggle_mixer_manual();
            break;
        case AuxFunction::LedToggleManual:
            toggle_led_manual();
            break;
    }
}

void SeederController::set_now_ms(std::uint32_t now_ms) {
    now_ms_ = now_ms;

    // Unsigned difference stays correct across the 2^32 ms wrap (about 49.7 days).
    const bool pulse_done = (now_ms_ - pulse_start_ms_) >= pulse_duration_ms_;
    if (section_state_ == SectionState::OpeningPulse && pulse_done) {
        section_state_ = SectionState::Open;
        section_is_open_ = true;
    } else if (section_state_ == SectionState::ClosingPulse && pulse_done) {
        section_state_ = SectionState::Closed;
        section_is_open_ = false;
    }
}

void SeederController::request_section_open() {
    section_state_ = SectionState::OpeningPulse;
    pulse_start_ms_ = now_ms_;
    pulse_duration_ms_ = open_pulse_duration_ms_;
}

void SeederController::request_section_close() {
    section_state_ = SectionState::ClosingPulse;
    pulse_start_ms_ = now_ms_;
    pulse_duration_ms_ = close_pulse_duration_ms_;
}

void SeederController::set_motor_manual(bool enabled) { motor_manual_ = enabled; }
void SeederController::set_mixer_manual(bool enabled) { mixer_manual_ = enabled; }
void SeederController::set_led_manual(bool enabled) { led_manual_ = enabled; }

void SeederController::toggle_motor_manual() { motor_manual_ = !motor_manual_; }
void SeederController::toggle_mixer_manual() { mixer_manual_ = !mixer_manual_; }
void SeederController::toggle_led_manual() { led_manual_ = !led_manual_; }

void SeederController::set_motor_auto_with_section(bool enabled) { motor_auto_with_section_ = enabled; }
void SeederController::set_mixer_auto_with_section(bool enabled) { mixer_auto_with_section_ = enabled; }

void SeederController::set_open_pulse_ms(std::uint32_t pulse_ms) { open_pulse_duration_ms_ = pulse_ms; }
void SeederController::set_close_pulse_ms(std::uint32_t pulse_ms) { close_pulse_duration_ms_ = pulse_ms; }

void SeederController::set_md13s_pwm_percent(int percent_0_100) { md13s_pwm_percent_ = clamp_percent(percent_0_100); }

bool SeederController::set_aux_assignment(std::uint8_t button, AuxFunction function) {
    if (!valid_button(button)) {
        return false;
    }
    aux_assignments_[button] = function;
    return true;
}

AuxFunction SeederController::get_aux_assignment(std::uint8_t button) const {
    if (!valid_button(button)) {
        return AuxFunction::None;
    }
    return aux_assignments_[button];
}

bool SeederController::clear_aux_assignment(std::uint8_t button) {
    return set_aux_assignment(button, AuxFunction::None);
}

void SeederController::handle_aux_button_event(std::uint8_t button, bool pressed) {
    if (!valid_button(button)) {
        return;
    }

    if (pressed && !aux_last_pressed_[button]) {
        apply_aux_function(aux_assignments_[button]);
    }
    aux_last_pressed_[button] = pressed;
}

void SeederController::prime_aux_button_state(std::uint8_t button, bool pressed) {
    if (!valid_button(button)) {
        return;
    }
    aux_last_pressed_[button] = pressed;
}

std::string SeederController::export_persistent_state() const {
    std::ostringstream out;
    out << "v1";
    out << ";openPulseMs=" << open_pulse_duration_ms_;
    out << ";closePulseMs=" << close_pulse_duration_ms_;
    out << ";motorAuto=" << (motor_auto_with_section_ ? 1 : 0);
    out << ";mixerAuto=" << (mixer_auto_with_section_ ? 1 : 0);
    out << ";motorManual=" << (motor_manual_ ? 1 : 0);
    out << ";mixerManual=" << (mixer_manual_ ? 1 : 0);
    out << ";ledManual=" << (led_manual_ ? 1 : 0);
    out << ";pwm=" << md13s_pwm_percent_;
    out << ";aux=";
    bool first = true;
    for (std::uint8_t button = 1; button <= kMaxAuxButtons; ++button) {
        if (aux_assignments_[button] == AuxFunction::None) {
            continue;
        }
        if (!first) {
            out << ",";
        }
        first = false;
        out << static_cast<int>(button) << ":" << aux_function_to_token(aux_assignments_[button]);
    }
    return out.str();
}

bool SeederController::import_persistent_state(const std::string& encoded) {
    if (encoded.rfind("v1;", 0) != 0) {
        return false;
    }

    SeederController imported(open_pulse_duration_ms_);
    imported.set_open_pulse_ms(open_pulse_duration_ms_);
    imported.set_close_pulse_ms(close_pulse_duration_ms_);

    std::size_t start = 3;
    while (start < encoded.size()) {
        std::size_t end = encoded.find(';', start);
        const std::string item = encoded.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (!item.empty()) {
            std::size_t eq = item.find('=');
            if (eq == std::string::npos) {
                return false;
            }
            const std::string key = item.substr(0, eq);
            const std::string value = item.substr(eq + 1);
            if (key == "openPulseMs") {
                std::uint32_t parsed = 0;
                if (!parse_u32(value, parsed)) return false;
                imported.open_pulse_duration_ms_ = parsed;
            } else if (key == "closePulseMs") {
                std::uint32_t parsed = 0;
                if (!parse_u32(value, parsed)) return false;
                imported.close_pulse_duration_ms_ = parsed;
            } else if (key == "motorAuto") {
                if (!parse_bool_int(value, imported.motor_auto_with_section_)) return false;
            } else if (key == "mixerAuto") {
                if (!parse_bool_int(value, imported.mixer_auto_with_section_)) return false;
            } else if (key == "motorManual") {
                if (!parse_bool_int(value, imported.motor_manual_)) return false;
            } else if (key == "mixerManual") {
                if (!parse_bool_int(value, imported.mixer_manual_)) return false;
            } else if (key == "ledManual") {
                if (!parse_bool_int(value, imported.led_manual_)) return false;
            } else if (key == "pwm") {
                int parsed = 0;
                if (!parse_int(value, parsed)) return false;
                imported.md13s_pwm_percent_ = clamp_percent(parsed);
            } else if (key == "aux") {
                if (!value.empty()) {
                    std::size_t auxStart = 0;
                    while (auxStart < value.size()) {
                        std::size_t auxEnd = value.find(',', auxStart);
                        std::string pair = value.substr(auxStart, auxEnd == std::string::npos ? std::string::npos : auxEnd - auxStart);
                        std::size_t colon = pair.find(':');
                        if (colon == std::string::npos) return false;
                        int button = 0;
                        if (!parse_int(pair.substr(0, colon), button)) return false;
                        AuxFunction function = AuxFunction::None;
                        if (!aux_function_from_token(pair.substr(colon + 1), function)) return false;
                        if (button < 1 || button > kMaxAuxButtons) return false;
                        imported.aux_assignments_[static_cast<std::uint8_t>(button)] = function;
                        if (auxEnd == std::string::npos) {
                            break;
                        }
                        auxStart = auxEnd + 1;
                    }
                }
            } else {
                return false;
            }
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }

    open_pulse_duration_ms_ = imported.open_pulse_duration_ms_;
    close_pulse_duration_ms_ = imported.close_pulse_duration_ms_;
    motor_auto_with_section_ = imported.motor_auto_with_section_;
    mixer_auto_with_section_ = imported.mixer_auto_with_section_;
    motor_manual_ = imported.motor_manual_;
    mixer_manual_ = imported.mixer_manual_;
    led_manual_ = imported.led_manual_;
    md13s_pwm_percent_ = imported.md13s_pwm_percent_;
    aux_assignments_ = imported.aux_assignments_;
    return true;
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

    out.md13s_pwm_percent = md13s_pwm_percent_;
    out.md13s_pin48_pwm_percent = out.relay_motor ? md13s_pwm_percent_ : 0;

    return out;
}

}  // namespace siewnik

