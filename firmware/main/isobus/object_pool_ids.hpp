#pragma once

#include <cstdint>

namespace iso::object_pool_ids {

constexpr std::uint16_t kTitleString = 1101;
inline std::uint16_t relay_fill_attr_id(int channel) { return static_cast<std::uint16_t>(1920 + channel); }

}  // namespace iso::object_pool_ids

