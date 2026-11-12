#pragma once

#include <memory>

#include "isobus/isobus/can_internal_control_function.hpp"

namespace siewnik {
class SeederController;
struct SeederSnapshot;
}  // namespace siewnik

namespace iso::vt_app {

void init(std::shared_ptr<isobus::InternalControlFunction> internal_ecu);
bool is_connected();
bool is_partner_claimed();
void apply_pending_commands(siewnik::SeederController& controller);
void publish_snapshot(const siewnik::SeederSnapshot& snapshot);

}  // namespace iso::vt_app

