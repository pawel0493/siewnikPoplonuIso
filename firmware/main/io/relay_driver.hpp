#pragma once

#include <cstdint>

namespace io::relay_driver {

bool init();
bool set_relay(std::uint8_t channel, bool on);
bool get_relay(std::uint8_t channel);

}  // namespace io::relay_driver

