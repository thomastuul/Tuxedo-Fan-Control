#include "fan_curve.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>

namespace {

struct CurvePoint {
  int temperature;
  int dutyPercent;
};

constexpr CurvePoint SILENT[] = {{0, 0},
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

constexpr CurvePoint QUIET[] = {
    {0, 0},   {51, 20}, {61, 22}, {64, 23}, {65, 24}, {66, 25}, {68, 28},
    {69, 30}, {70, 33}, {71, 37}, {72, 40}, {73, 43}, {74, 44}, {75, 46},
    {76, 48}, {77, 52}, {79, 55}, {81, 60}, {83, 65}, {85, 70}, {87, 80},
    {89, 85}, {91, 90}, {93, 95}, {95, 100}};

constexpr CurvePoint BALANCED[] = {
    {0, 0},   {46, 20}, {52, 23}, {54, 26}, {57, 30}, {60, 33}, {63, 35},
    {65, 38}, {66, 40}, {67, 42}, {68, 45}, {69, 47}, {70, 50}, {72, 52},
    {73, 53}, {75, 57}, {77, 60}, {79, 63}, {80, 65}, {82, 70}, {84, 75},
    {86, 80}, {88, 85}, {89, 90}, {92, 95}, {95, 100}};

constexpr CurvePoint COOL[] = {
    {0, 0},   {40, 20}, {46, 25}, {51, 30}, {56, 32}, {57, 33}, {58, 34},
    {59, 35}, {61, 37}, {62, 40}, {64, 42}, {65, 45}, {68, 47}, {69, 50},
    {71, 52}, {72, 55}, {74, 57}, {75, 60}, {77, 65}, {79, 70}, {81, 75},
    {83, 80}, {85, 85}, {87, 90}, {90, 95}, {95, 100}};

constexpr CurvePoint FREEZY[] = {{0, 20},
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

template <std::size_t SIZE>
int dutyPercentForTemperature(int temperature,
                              const CurvePoint (&curve)[SIZE]) {
  int dutyPercent = curve[0].dutyPercent;
  for (std::size_t index = 1; index < SIZE; ++index) {
    if (temperature < curve[index].temperature) {
      break;
    }
    dutyPercent = curve[index].dutyPercent;
  }
  return dutyPercent;
}

int dutyPercentForTemperature(int temperature, FanProfile profile) {
  switch (profile) {
  case FanProfile::Silent:
    return dutyPercentForTemperature(temperature, SILENT);
  case FanProfile::Quiet:
    return dutyPercentForTemperature(temperature, QUIET);
  case FanProfile::Cool:
    return dutyPercentForTemperature(temperature, COOL);
  case FanProfile::Freezy:
    return dutyPercentForTemperature(temperature, FREEZY);
  case FanProfile::Balanced:
  default:
    return dutyPercentForTemperature(temperature, BALANCED);
  }
}

} // namespace

FanProfile fanProfileFromName(const char *name) {
  if (name == nullptr) {
    return FanProfile::Balanced;
  }
  if (std::strcmp(name, "silent") == 0) {
    return FanProfile::Silent;
  }
  if (std::strcmp(name, "quiet") == 0) {
    return FanProfile::Quiet;
  }
  if (std::strcmp(name, "cool") == 0) {
    return FanProfile::Cool;
  }
  if (std::strcmp(name, "freezy") == 0) {
    return FanProfile::Freezy;
  }
  return FanProfile::Balanced;
}

FanProfile fanProfileFromCommandLine(int argc, const char *const argv[]) {
  if (argc == 3 && std::strcmp(argv[1], "--profile") == 0) {
    return fanProfileFromName(argv[2]);
  }
  return FanProfile::Balanced;
}

const char *fanProfileName(FanProfile profile) {
  switch (profile) {
  case FanProfile::Silent:
    return "silent";
  case FanProfile::Quiet:
    return "quiet";
  case FanProfile::Cool:
    return "cool";
  case FanProfile::Freezy:
    return "freezy";
  case FanProfile::Balanced:
  default:
    return "balanced";
  }
}

int calculateDynamicFanSpeed(int temperature, FanProfile profile) {
  const int DUTY_PERCENT = dutyPercentForTemperature(temperature, profile);
  return static_cast<int>(
      std::round(static_cast<float>(DUTY_PERCENT) / 100 * FAN_MAX_SPEED));
}

int clampFanSpeed(int speed) {
  return std::min(std::max(speed, FAN_MIN_VALUE), FAN_MAX_SPEED);
}
