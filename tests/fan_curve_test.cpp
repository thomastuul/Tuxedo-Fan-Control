#include "fan_curve.h"

#include <cmath>
#include <iostream>

namespace {

int failures = 0;

struct ExpectedPoint {
  int temperature;
  int dutyPercent;
};

void expectEqual(const char *name, int expected, int actual) {
  if (expected == actual) {
    return;
  }

  std::cerr << name << ": expected " << expected << ", got " << actual << '\n';
  ++failures;
}

void expectProfile(const char *name, FanProfile expected, FanProfile actual) {
  expectEqual(name, static_cast<int>(expected), static_cast<int>(actual));
}

int speedForPercent(int percent) {
  return static_cast<int>(
      std::lround(static_cast<double>(percent) / 100 * FAN_MAX_SPEED));
}

template <std::size_t SIZE>
void testCurve(const char *name,
               FanProfile profile,
               const ExpectedPoint (&points)[SIZE]) {
  expectEqual(name,
              speedForPercent(points[0].dutyPercent),
              calculateDynamicFanSpeed(-40, profile));
  for (std::size_t index = 0; index < SIZE; ++index) {
    expectEqual(name,
                speedForPercent(points[index].dutyPercent),
                calculateDynamicFanSpeed(points[index].temperature, profile));
    if (index > 0) {
      expectEqual(
          name,
          speedForPercent(points[index - 1].dutyPercent),
          calculateDynamicFanSpeed(points[index].temperature - 1, profile));
    }
  }
  expectEqual(name, FAN_MAX_SPEED, calculateDynamicFanSpeed(101, profile));
}

const ExpectedPoint SILENT[] = {{0, 0},
                                {61, 20},
                                {66, 25},
                                {70, 30},
                                {72, 35},
                                {74, 40},
                                {76, 45},
                                {78, 50},
                                {80, 55},
                                {82, 60},
                                {84, 65},
                                {86, 70},
                                {88, 75},
                                {89, 80},
                                {90, 85},
                                {91, 90},
                                {93, 95},
                                {95, 100}};
const ExpectedPoint QUIET[] = {
    {0, 0},   {51, 20}, {61, 22}, {64, 23}, {65, 24}, {66, 25}, {68, 28},
    {69, 30}, {70, 33}, {71, 37}, {72, 40}, {73, 43}, {74, 44}, {75, 46},
    {76, 48}, {77, 52}, {79, 55}, {81, 60}, {83, 65}, {85, 70}, {87, 80},
    {89, 85}, {91, 90}, {93, 95}, {95, 100}};
const ExpectedPoint BALANCED[] = {
    {0, 0},   {46, 20}, {52, 23}, {54, 26}, {57, 30}, {60, 33}, {63, 35},
    {65, 38}, {66, 40}, {67, 42}, {68, 45}, {69, 47}, {70, 50}, {72, 52},
    {73, 53}, {75, 57}, {77, 60}, {79, 63}, {80, 65}, {82, 70}, {84, 75},
    {86, 80}, {88, 85}, {89, 90}, {92, 95}, {95, 100}};
const ExpectedPoint COOL[] = {
    {0, 0},   {40, 20}, {46, 25}, {51, 30}, {56, 32}, {57, 33}, {58, 34},
    {59, 35}, {61, 37}, {62, 40}, {64, 42}, {65, 45}, {68, 47}, {69, 50},
    {71, 52}, {72, 55}, {74, 57}, {75, 60}, {77, 65}, {79, 70}, {81, 75},
    {83, 80}, {85, 85}, {87, 90}, {90, 95}, {95, 100}};
const ExpectedPoint FREEZY[] = {{0, 20},
                                {30, 25},
                                {40, 30},
                                {46, 35},
                                {50, 40},
                                {56, 45},
                                {61, 50},
                                {66, 55},
                                {71, 60},
                                {76, 65},
                                {78, 70},
                                {80, 75},
                                {82, 80},
                                {84, 85},
                                {86, 90},
                                {90, 95},
                                {95, 100}};

} // namespace

int runEcControllerTests();

int main() {
  expectProfile(
      "silent name", FanProfile::Silent, fanProfileFromName("silent"));
  expectProfile("quiet name", FanProfile::Quiet, fanProfileFromName("quiet"));
  expectProfile(
      "balanced name", FanProfile::Balanced, fanProfileFromName("balanced"));
  expectProfile("cool name", FanProfile::Cool, fanProfileFromName("cool"));
  expectProfile(
      "freezy name", FanProfile::Freezy, fanProfileFromName("freezy"));
  expectProfile("invalid name defaults",
                FanProfile::Balanced,
                fanProfileFromName("invalid"));
  expectProfile(
      "null name defaults", FanProfile::Balanced, fanProfileFromName(nullptr));

  const char *validArguments[] = {"program", "--profile", "quiet"};
  const char *invalidArguments[] = {"program", "--other", "cool"};
  const char *invalidValue[] = {"program", "--profile", "invalid"};
  const char *missingValue[] = {"program", "--profile"};
  expectProfile("valid arguments",
                FanProfile::Quiet,
                fanProfileFromCommandLine(3, validArguments));
  expectProfile("invalid option defaults",
                FanProfile::Balanced,
                fanProfileFromCommandLine(3, invalidArguments));
  expectProfile("invalid value defaults",
                FanProfile::Balanced,
                fanProfileFromCommandLine(3, invalidValue));
  expectProfile("missing value defaults",
                FanProfile::Balanced,
                fanProfileFromCommandLine(2, missingValue));
  expectProfile("no arguments defaults",
                FanProfile::Balanced,
                fanProfileFromCommandLine(1, validArguments));

  testCurve("silent curve", FanProfile::Silent, SILENT);
  testCurve("quiet curve", FanProfile::Quiet, QUIET);
  testCurve("balanced curve", FanProfile::Balanced, BALANCED);
  testCurve("cool curve", FanProfile::Cool, COOL);
  testCurve("freezy curve", FanProfile::Freezy, FREEZY);

  expectEqual("speed below minimum", FAN_MIN_VALUE, clampFanSpeed(-1));
  expectEqual("zero speed", FAN_MIN_VALUE, clampFanSpeed(0));
  expectEqual("minimum speed", FAN_MIN_VALUE, clampFanSpeed(FAN_MIN_VALUE));
  expectEqual("normal speed", 128, clampFanSpeed(128));
  expectEqual("maximum speed", FAN_MAX_SPEED, clampFanSpeed(FAN_MAX_SPEED));
  expectEqual(
      "speed above maximum", FAN_MAX_SPEED, clampFanSpeed(FAN_MAX_SPEED + 45));

  failures += runEcControllerTests();
  return failures == 0 ? 0 : 1;
}
