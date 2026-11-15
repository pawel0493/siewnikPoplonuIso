#include "storage/aux_assignment_store.hpp"

#include <algorithm>
#include <utility>

#include "esp_log.h"
#include "nvs.h"

namespace storage::aux_assignment_store {

namespace {
constexpr const char* kTag = "aux_assign_store";
constexpr const char* kNamespace = "seeder";
constexpr const char* kKey = "aux_assign";

// A single physical AUX-N input device (for example, one joystick) rarely needs more than a
// handful of assigned functions, and this project expects at most a couple of input devices.
constexpr std::size_t kMaxDevices = 2;
constexpr std::size_t kMaxFunctionsPerDevice = 8;
constexpr std::size_t kFunctionRecordSize = 5;  // functionObjectID(2) + inputObjectID(2) + functionType(1)

struct StoredDevice {
    std::uint64_t name = 0;
    std::uint16_t model_id = 0;
    std::vector<isobus::VirtualTerminalClient::AssignedAuxiliaryFunction> functions;
};

void write_u16(std::vector<std::uint8_t>& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>(value));
    out.push_back(static_cast<std::uint8_t>(value >> 8));
}

void write_u64(std::vector<std::uint8_t>& out, std::uint64_t value) {
    for (int i = 0; i < 8; ++i) {
        out.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
    }
}

std::uint16_t read_u16(const std::uint8_t* data) {
    return static_cast<std::uint16_t>(data[0]) | (static_cast<std::uint16_t>(data[1]) << 8);
}

std::uint64_t read_u64(const std::uint8_t* data) {
    std::uint64_t value = 0;
    for (int i = 0; i < 8; ++i) {
        value |= static_cast<std::uint64_t>(data[i]) << (8 * i);
    }
    return value;
}

std::vector<std::uint8_t> serialize(const std::vector<StoredDevice>& devices) {
    std::vector<std::uint8_t> out;
    out.push_back(static_cast<std::uint8_t>(devices.size()));
    for (const auto& device : devices) {
        write_u64(out, device.name);
        write_u16(out, device.model_id);
        const std::size_t count = std::min(device.functions.size(), kMaxFunctionsPerDevice);
        out.push_back(static_cast<std::uint8_t>(count));
        for (std::size_t i = 0; i < count; ++i) {
            const auto& function = device.functions[i];
            write_u16(out, function.functionObjectID);
            write_u16(out, function.inputObjectID);
            out.push_back(static_cast<std::uint8_t>(function.functionType));
        }
    }
    return out;
}

// Parses a previously serialized blob. Returns an empty list if the blob is missing, truncated,
// or otherwise malformed, so a corrupt record is treated as "nothing stored" rather than crashing.
std::vector<StoredDevice> deserialize(const std::vector<std::uint8_t>& blob) {
    std::vector<StoredDevice> devices;
    if (blob.empty()) {
        return devices;
    }

    std::size_t pos = 0;
    const std::size_t device_count = std::min<std::size_t>(blob[pos], kMaxDevices);
    pos += 1;

    for (std::size_t d = 0; d < device_count; ++d) {
        if (pos + 8 + 2 + 1 > blob.size()) {
            ESP_LOGW(kTag, "Stored AUX-N assignments truncated, ignoring remainder");
            break;
        }
        StoredDevice device;
        device.name = read_u64(&blob[pos]);
        pos += 8;
        device.model_id = read_u16(&blob[pos]);
        pos += 2;
        const std::size_t function_count = std::min<std::size_t>(blob[pos], kMaxFunctionsPerDevice);
        pos += 1;

        for (std::size_t f = 0; f < function_count; ++f) {
            if (pos + kFunctionRecordSize > blob.size()) {
                ESP_LOGW(kTag, "Stored AUX-N assignments truncated, ignoring remainder");
                break;
            }
            const std::uint16_t function_object_id = read_u16(&blob[pos]);
            const std::uint16_t input_object_id = read_u16(&blob[pos + 2]);
            const auto function_type = static_cast<isobus::VirtualTerminalClient::AuxiliaryTypeTwoFunctionType>(blob[pos + 4]);
            device.functions.emplace_back(function_object_id, input_object_id, function_type);
            pos += kFunctionRecordSize;
        }
        devices.push_back(std::move(device));
    }
    return devices;
}

bool load_blob(std::vector<std::uint8_t>& out_blob) {
    nvs_handle_t handle = 0;
    if (nvs_open(kNamespace, NVS_READONLY, &handle) != ESP_OK) {
        return false;
    }
    size_t size = 0;
    esp_err_t err = nvs_get_blob(handle, kKey, nullptr, &size);
    if (err != ESP_OK || size == 0) {
        nvs_close(handle);
        return false;
    }
    out_blob.resize(size);
    err = nvs_get_blob(handle, kKey, out_blob.data(), &size);
    nvs_close(handle);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "nvs_get_blob failed: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}

void save_blob(const std::vector<std::uint8_t>& blob) {
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(kNamespace, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "nvs_open(save) failed: %s", esp_err_to_name(err));
        return;
    }
    err = nvs_set_blob(handle, kKey, blob.data(), blob.size());
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to save AUX-N assignments: %s", esp_err_to_name(err));
    }
}
}  // namespace

std::vector<isobus::VirtualTerminalClient::AssignedAuxiliaryFunction> load(std::uint64_t device_name,
                                                                            std::uint16_t model_id) {
    std::vector<std::uint8_t> blob;
    if (!load_blob(blob)) {
        return {};
    }
    for (const auto& device : deserialize(blob)) {
        if (device.name == device_name && device.model_id == model_id) {
            return device.functions;
        }
    }
    return {};
}

void save(std::uint64_t device_name,
          std::uint16_t model_id,
          const std::vector<isobus::VirtualTerminalClient::AssignedAuxiliaryFunction>& assignments) {
    std::vector<std::uint8_t> blob;
    load_blob(blob);
    std::vector<StoredDevice> devices = deserialize(blob);

    auto it = std::find_if(devices.begin(), devices.end(), [&](const StoredDevice& device) {
        return device.name == device_name && device.model_id == model_id;
    });
    if (it != devices.end()) {
        it->functions = assignments;
    } else {
        if (devices.size() >= kMaxDevices) {
            devices.erase(devices.begin());  // Drop the oldest device to make room.
        }
        devices.push_back(StoredDevice{device_name, model_id, assignments});
    }

    save_blob(serialize(devices));
}

}  // namespace storage::aux_assignment_store
