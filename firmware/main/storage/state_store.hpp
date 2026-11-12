#pragma once

#include <string>

namespace storage::state_store {

bool load(std::string& out_encoded_state);
bool save(const std::string& encoded_state);

}  // namespace storage::state_store

