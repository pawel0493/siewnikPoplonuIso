#include <cassert>
#include <cstdint>
#include <string>

#include "seeder_controller.hpp"

using siewnik::AuxFunction;
using siewnik::SectionState;
using siewnik::SeederController;

static void test_open_close_pulse() {
    SeederController c(500);
    c.set_now_ms(1000);
    c.request_section_open();
    auto s = c.snapshot();
    assert(s.relay_open_pulse);
    assert(!s.section_is_open);
    assert(s.section_state == SectionState::OpeningPulse);

    c.set_now_ms(1499);
    s = c.snapshot();
    assert(s.relay_open_pulse);
    assert(!s.section_is_open);

    c.set_now_ms(1500);
    s = c.snapshot();
    assert(!s.relay_open_pulse);
    assert(s.section_is_open);
    assert(s.section_state == SectionState::Open);

    c.request_section_close();
    s = c.snapshot();
    assert(s.relay_close_pulse);
    assert(s.section_state == SectionState::ClosingPulse);

    c.set_now_ms(2000);
    s = c.snapshot();
    assert(!s.relay_close_pulse);
    assert(!s.section_is_open);
    assert(s.section_state == SectionState::Closed);
}

static void test_independent_pulse_config() {
    SeederController c(500);
    c.set_open_pulse_ms(700);
    c.set_close_pulse_ms(300);
    c.set_now_ms(1000);

    c.request_section_open();
    c.set_now_ms(1699);
    auto s = c.snapshot();
    assert(s.relay_open_pulse);
    assert(!s.section_is_open);

    c.set_now_ms(1700);
    s = c.snapshot();
    assert(!s.relay_open_pulse);
    assert(s.section_is_open);

    c.request_section_close();
    c.set_now_ms(1999);
    s = c.snapshot();
    assert(s.relay_close_pulse);
    assert(s.section_is_open);

    c.set_now_ms(2000);
    s = c.snapshot();
    assert(!s.relay_close_pulse);
    assert(!s.section_is_open);

    assert(s.open_pulse_ms == 700);
    assert(s.close_pulse_ms == 300);
}

static void test_auto_manual_relays() {
    SeederController c(500);
    c.set_now_ms(0);

    c.set_motor_manual(true);
    c.set_mixer_manual(true);
    auto s = c.snapshot();
    assert(s.relay_motor);
    assert(s.relay_mixer);

    c.set_motor_auto_with_section(true);
    c.set_mixer_auto_with_section(true);
    s = c.snapshot();
    assert(!s.relay_motor);
    assert(!s.relay_mixer);

    c.request_section_open();
    c.set_now_ms(500);
    s = c.snapshot();
    assert(s.section_is_open);
    assert(s.relay_motor);
    assert(s.relay_mixer);
}

static void test_md13s_pwm_output() {
    SeederController c(500);
    c.set_md13s_pwm_percent(140);
    auto s = c.snapshot();
    assert(s.md13s_pwm_percent == 100);
    assert(s.md13s_pin48_pwm_percent == 0);

    c.set_motor_manual(true);
    s = c.snapshot();
    assert(s.md13s_pin48_pwm_percent == 100);

    c.set_md13s_pwm_percent(-10);
    s = c.snapshot();
    assert(s.md13s_pwm_percent == 0);
    assert(s.md13s_pin48_pwm_percent == 0);
}

static void test_aux_assignment_edge_trigger() {
    SeederController c(500);
    assert(c.set_aux_assignment(1, AuxFunction::SectionOpenPulse));
    assert(c.set_aux_assignment(2, AuxFunction::SectionClosePulse));
    assert(c.set_aux_assignment(3, AuxFunction::LedToggleManual));
    assert(c.get_aux_assignment(1) == AuxFunction::SectionOpenPulse);
    assert(!c.set_aux_assignment(0, AuxFunction::SectionToggle));

    c.set_now_ms(1000);
    c.handle_aux_button_event(1, true);
    auto s = c.snapshot();
    assert(s.relay_open_pulse);

    c.handle_aux_button_event(1, true);
    s = c.snapshot();
    assert(s.relay_open_pulse);  // no duplicate trigger while held

    c.handle_aux_button_event(1, false);
    c.set_now_ms(1500);
    s = c.snapshot();
    assert(s.section_is_open);

    c.handle_aux_button_event(3, true);
    s = c.snapshot();
    assert(s.relay_led);
    c.handle_aux_button_event(3, false);
    c.handle_aux_button_event(3, true);
    s = c.snapshot();
    assert(!s.relay_led);
}

static void test_persistent_state_roundtrip() {
    SeederController a(500);
    a.set_open_pulse_ms(620);
    a.set_close_pulse_ms(410);
    a.set_motor_auto_with_section(true);
    a.set_mixer_auto_with_section(false);
    a.set_motor_manual(true);
    a.set_mixer_manual(true);
    a.set_led_manual(true);
    a.set_md13s_pwm_percent(73);
    assert(a.set_aux_assignment(1, AuxFunction::SectionToggle));
    assert(a.set_aux_assignment(4, AuxFunction::LedToggleManual));

    const std::string persisted = a.export_persistent_state();

    SeederController b(500);
    assert(b.import_persistent_state(persisted));
    auto s = b.snapshot();
    assert(s.open_pulse_ms == 620);
    assert(s.close_pulse_ms == 410);
    assert(s.motor_auto_with_section);
    assert(!s.mixer_auto_with_section);
    assert(!s.relay_motor);
    assert(s.relay_mixer);
    assert(s.relay_led);
    assert(s.md13s_pwm_percent == 73);
    assert(b.get_aux_assignment(1) == AuxFunction::SectionToggle);
    assert(b.get_aux_assignment(4) == AuxFunction::LedToggleManual);
    assert(!b.import_persistent_state("invalid"));
}

int main() {
    test_open_close_pulse();
    test_independent_pulse_config();
    test_auto_manual_relays();
    test_md13s_pwm_output();
    test_aux_assignment_edge_trigger();
    test_persistent_state_roundtrip();
    return 0;
}

