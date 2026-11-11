#include <cassert>
#include <cstdint>

#include "seeder_controller.hpp"

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

static void test_md13s_setpoint_effective() {
    SeederController c(500);
    c.set_md13s_setpoint_percent(140);
    auto s = c.snapshot();
    assert(s.md13s_setpoint_percent == 100);
    assert(s.md13s_effective_percent == 0);

    c.set_motor_manual(true);
    s = c.snapshot();
    assert(s.md13s_effective_percent == 100);

    c.set_md13s_setpoint_percent(-10);
    s = c.snapshot();
    assert(s.md13s_setpoint_percent == 0);
    assert(s.md13s_effective_percent == 0);
}

int main() {
    test_open_close_pulse();
    test_auto_manual_relays();
    test_md13s_setpoint_effective();
    return 0;
}

