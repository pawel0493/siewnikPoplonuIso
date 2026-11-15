#pragma once

#include <cstdint>
#include <vector>

#include "isobus/isobus/isobus_virtual_terminal_client.hpp"

namespace storage::aux_assignment_store {

// Loads the AUX-N function assignments stored for one input device (for example, a joystick),
// identified by its NAME and model identification code. Returns an empty vector if nothing is
// stored for that device yet.
std::vector<isobus::VirtualTerminalClient::AssignedAuxiliaryFunction> load(std::uint64_t device_name,
                                                                            std::uint16_t model_id);

// Persists the AUX-N function assignments for one input device to NVS, so they survive a restart.
// A small, fixed number of devices and assignments is kept; see the .cpp for the limits.
void save(std::uint64_t device_name,
          std::uint16_t model_id,
          const std::vector<isobus::VirtualTerminalClient::AssignedAuxiliaryFunction>& assignments);

}  // namespace storage::aux_assignment_store
