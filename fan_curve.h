#pragma once

enum class FanProfile { Silent, Quiet, Balanced, Cool, Freezy };

constexpr int FAN_MIN_VALUE = 0;
constexpr int FAN_MAX_SPEED = 255;

FanProfile fanProfileFromName(const char *name);
FanProfile fanProfileFromCommandLine(int argc, const char *const argv[]);
bool isKnownFanProfileName(const char *name);
const char *fanProfileName(FanProfile profile);
int calculateDynamicFanSpeed(int temperature, FanProfile profile);
int clampFanSpeed(int speed);
