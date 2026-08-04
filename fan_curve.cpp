#include "fan_curve.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace {

constexpr std::array<int, FAN_TABLE_MAX_TEMP - FAN_TABLE_MIN_TEMP + 1>
    TUXEDO_FAN_DUTY_PERCENT = {{
        10, 10, 10, 12, 12, 12, 15, 15, 15, 17, 17, 17, 19, 19, 19,
        22, 23, 24, 25, 27, 29, 35, 35, 37, 37, 42, 42, 45, 45, 45,
        50, 50, 55, 55, 60, 60, 70, 70, 75, 80, 80, 85, 85, 85, 90,
        90, 90, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100
    }};

int dutyPercentForTemperature(int temperature) {
  if (temperature < FAN_TABLE_MIN_TEMP) {
    return FAN_DEFAULT_DUTY_PERCENT;
  }

  if (temperature > FAN_TABLE_MAX_TEMP) {
    return FAN_MAX_DUTY_PERCENT;
  }

  return TUXEDO_FAN_DUTY_PERCENT[temperature - FAN_TABLE_MIN_TEMP];
}

} // namespace

int calculateDynamicFanSpeed(int temperature) {
  const int dutyPercent = dutyPercentForTemperature(temperature);
  return static_cast<int>(std::round(
      static_cast<float>(dutyPercent) / FAN_MAX_DUTY_PERCENT * FAN_MAX_SPEED));
}

int clampFanSpeed(int speed) {
  return std::min(std::max(speed, FAN_MIN_VALUE), FAN_MAX_SPEED);
}
