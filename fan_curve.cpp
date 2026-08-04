#include "fan_curve.h"

#include <algorithm>
#include <cmath>

int calculateDynamicFanSpeed(int temperature) {
  int dynamicFanSpeed = static_cast<int>(
      std::round(static_cast<float>(temperature - FAN_OFF_TEMP) /
                     (FAN_MAX_TEMP - FAN_OFF_TEMP) * (255 - FAN_START_VALUE) +
                 FAN_START_VALUE));

  if (dynamicFanSpeed < FAN_START_VALUE) {
    return 0;
  }

  return std::min(dynamicFanSpeed, 255);
}

int clampFanSpeed(int speed) {
  return std::min(std::max(speed, FAN_MIN_VALUE), 255);
}
