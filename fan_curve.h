#pragma once

constexpr int FAN_MIN_VALUE = 40;
constexpr int FAN_OFF_TEMP = 70;
constexpr int FAN_MAX_TEMP = 90;
constexpr int FAN_START_VALUE = 100;
constexpr int FAN_PEAK_HOLD_TIME = 10000;

int calculateDynamicFanSpeed(int temperature);
int clampFanSpeed(int speed);
