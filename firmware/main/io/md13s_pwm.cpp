#include "io/md13s_pwm.hpp"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"

namespace io::md13s_pwm {

namespace {
constexpr gpio_num_t kPwmPin = GPIO_NUM_48;
constexpr ledc_mode_t kMode = LEDC_LOW_SPEED_MODE;
constexpr ledc_timer_t kTimer = LEDC_TIMER_0;
constexpr ledc_channel_t kChannel = LEDC_CHANNEL_0;
constexpr ledc_timer_bit_t kResolution = LEDC_TIMER_13_BIT;
constexpr std::uint32_t kFreqHz = 2000;
constexpr int kMaxDuty = (1 << 13) - 1;
constexpr const char* kTag = "md13s_pwm";

int g_percent = 0;
bool g_ready = false;
}  // namespace

bool init() {
    ledc_timer_config_t timer_cfg = {};
    timer_cfg.speed_mode = kMode;
    timer_cfg.timer_num = kTimer;
    timer_cfg.duty_resolution = kResolution;
    timer_cfg.freq_hz = kFreqHz;
    timer_cfg.clk_cfg = LEDC_AUTO_CLK;
    if (ledc_timer_config(&timer_cfg) != ESP_OK) {
        ESP_LOGE(kTag, "ledc_timer_config failed");
        return false;
    }

    ledc_channel_config_t ch_cfg = {};
    ch_cfg.gpio_num = kPwmPin;
    ch_cfg.speed_mode = kMode;
    ch_cfg.channel = kChannel;
    ch_cfg.intr_type = LEDC_INTR_DISABLE;
    ch_cfg.timer_sel = kTimer;
    ch_cfg.duty = 0;
    ch_cfg.hpoint = 0;
    if (ledc_channel_config(&ch_cfg) != ESP_OK) {
        ESP_LOGE(kTag, "ledc_channel_config failed");
        return false;
    }

    g_percent = 0;
    g_ready = true;
    return true;
}

void set_percent(int percent_0_100) {
    if (!g_ready) {
        return;
    }
    if (percent_0_100 < 0) {
        percent_0_100 = 0;
    } else if (percent_0_100 > 100) {
        percent_0_100 = 100;
    }
    if (percent_0_100 == g_percent) {
        return;
    }
    g_percent = percent_0_100;
    const int duty = (kMaxDuty * g_percent) / 100;
    ledc_set_duty(kMode, kChannel, duty);
    ledc_update_duty(kMode, kChannel);
}

int get_percent() {
    return g_percent;
}

}  // namespace io::md13s_pwm

