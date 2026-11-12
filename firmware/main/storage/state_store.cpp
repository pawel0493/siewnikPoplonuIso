#include "storage/state_store.hpp"

#include <vector>

#include "esp_log.h"
#include "nvs.h"

namespace storage::state_store {

namespace {
constexpr const char* kTag = "state_store";
constexpr const char* kNamespace = "seeder";
constexpr const char* kKey = "ctrl_state";
}  // namespace

bool load(std::string& out_encoded_state) {
    out_encoded_state.clear();

    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(kNamespace, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "nvs_open(load) failed: %s", esp_err_to_name(err));
        return false;
    }

    size_t size = 0;
    err = nvs_get_str(handle, kKey, nullptr, &size);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(handle);
        return false;
    }
    if (err != ESP_OK || size == 0) {
        ESP_LOGW(kTag, "nvs_get_str(size) failed: %s", esp_err_to_name(err));
        nvs_close(handle);
        return false;
    }

    std::vector<char> buffer(size);
    err = nvs_get_str(handle, kKey, buffer.data(), &size);
    nvs_close(handle);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "nvs_get_str(data) failed: %s", esp_err_to_name(err));
        return false;
    }

    out_encoded_state.assign(buffer.data());
    return true;
}

bool save(const std::string& encoded_state) {
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(kNamespace, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "nvs_open(save) failed: %s", esp_err_to_name(err));
        return false;
    }

    err = nvs_set_str(handle, kKey, encoded_state.c_str());
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "nvs save failed: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}

}  // namespace storage::state_store

