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
  expectEqual("temperature below table", 3, calculateDynamicFanSpeed(-40));
  expectEqual(
      "table lower boundary", 26, calculateDynamicFanSpeed(FAN_TABLE_MIN_TEMP));
  expectEqual("65 degree entry", 89, calculateDynamicFanSpeed(65));
  expectEqual("70 degree entry", 107, calculateDynamicFanSpeed(70));
  expectEqual("80 degree entry", 179, calculateDynamicFanSpeed(80));
  expectEqual("90 degree entry", 230, calculateDynamicFanSpeed(90));
  expectEqual("table upper boundary",
              FAN_MAX_SPEED,
              calculateDynamicFanSpeed(FAN_TABLE_MAX_TEMP));
  expectEqual("temperature above table",
              FAN_MAX_SPEED,
              calculateDynamicFanSpeed(FAN_TABLE_MAX_TEMP + 1));

  expectEqual("speed below minimum", FAN_MIN_VALUE, clampFanSpeed(-1));
  expectEqual("zero speed", FAN_MIN_VALUE, clampFanSpeed(0));
  expectEqual("minimum speed", FAN_MIN_VALUE, clampFanSpeed(FAN_MIN_VALUE));
  expectEqual("normal speed", 128, clampFanSpeed(128));
  expectEqual("maximum speed", FAN_MAX_SPEED, clampFanSpeed(FAN_MAX_SPEED));
  expectEqual(
      "speed above maximum", FAN_MAX_SPEED, clampFanSpeed(FAN_MAX_SPEED + 45));

  return failures == 0 ? 0 : 1;
}
