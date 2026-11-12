#include "io/input_driver.hpp"

#include "driver/gpio.h"

namespace io::input_driver {

namespace {
constexpr gpio_num_t kPins[8] = {
    GPIO_NUM_4, GPIO_NUM_5, GPIO_NUM_6, GPIO_NUM_7,
    GPIO_NUM_8, GPIO_NUM_9, GPIO_NUM_10, GPIO_NUM_11,
};

bool g_debounced_state[8] = {};
std::uint8_t g_match_count[8] = {};
}  // namespace

void init() {
    std::uint64_t pin_mask = 0;
    for (gpio_num_t pin : kPins) {
        pin_mask |= (1ULL << pin);
    }

    gpio_config_t cfg = {};
    cfg.pin_bit_mask = pin_mask;
    cfg.mode = GPIO_MODE_INPUT;
    cfg.pull_up_en = GPIO_PULLUP_ENABLE;
    cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    cfg.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&cfg);
}

void update() {
    for (int i = 0; i < 8; ++i) {
        const bool raw_active = (gpio_get_level(kPins[i]) == 0);  // board DI is active-low
        if (raw_active == g_debounced_state[i]) {
            g_match_count[i] = 0;
            continue;
        }
        if (++g_match_count[i] >= kDebounceSamples) {
            g_debounced_state[i] = raw_active;
            g_match_count[i] = 0;
        }
    }
}

bool read_debounced(std::uint8_t channel) {
    if (channel < 1 || channel > 8) {
        return false;
    }
    return g_debounced_state[channel - 1];
}

}  // namespace io::input_driver

