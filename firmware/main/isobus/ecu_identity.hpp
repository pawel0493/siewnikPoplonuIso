#pragma once

#include <memory>

#include "isobus/isobus/can_internal_control_function.hpp"

namespace iso::ecu_identity {

std::shared_ptr<isobus::InternalControlFunction> init();

}  // namespace iso::ecu_identity

