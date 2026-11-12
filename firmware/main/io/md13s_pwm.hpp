#pragma once

namespace io::md13s_pwm {

bool init();
void set_percent(int percent_0_100);
int get_percent();

}  // namespace io::md13s_pwm

