#pragma once

#include <cstdint>

namespace io::input_driver {

constexpr std::uint8_t kDebounceSamples = 3;

void init();
void update();
bool read_debounced(std::uint8_t channel);

}  // namespace io::input_driver

