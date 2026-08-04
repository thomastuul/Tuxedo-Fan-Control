#include "fan_curve.h"

#include <iostream>

namespace {

int failures = 0;

void expectEqual(const char *name, int expected, int actual) {
  if (expected == actual) {
    return;
  }

  std::cerr << name << ": expected " << expected << ", got " << actual << '\n';
  ++failures;
}

} // namespace

int main() {
  expectEqual("temperature below off threshold",
              0,
              calculateDynamicFanSpeed(FAN_OFF_TEMP - 1));
  expectEqual("minimum temperature input", 0, calculateDynamicFanSpeed(-40));
  expectEqual(
      "off threshold", FAN_START_VALUE, calculateDynamicFanSpeed(FAN_OFF_TEMP));
  expectEqual(
      "maximum temperature", 255, calculateDynamicFanSpeed(FAN_MAX_TEMP));
  expectEqual("temperature above maximum",
              255,
              calculateDynamicFanSpeed(FAN_MAX_TEMP + 1));

  expectEqual("speed below minimum", FAN_MIN_VALUE, clampFanSpeed(-1));
  expectEqual("zero speed", FAN_MIN_VALUE, clampFanSpeed(0));
  expectEqual("minimum speed", FAN_MIN_VALUE, clampFanSpeed(FAN_MIN_VALUE));
  expectEqual("normal speed", 128, clampFanSpeed(128));
  expectEqual("maximum speed", 255, clampFanSpeed(255));
  expectEqual("speed above maximum", 255, clampFanSpeed(300));

  return failures == 0 ? 0 : 1;
}
