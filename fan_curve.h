#pragma once

constexpr int FAN_MIN_VALUE = 1;
constexpr int FAN_TABLE_MIN_TEMP = 44;
constexpr int FAN_TABLE_MAX_TEMP = 100;
constexpr int FAN_DEFAULT_DUTY_PERCENT = 1;
constexpr int FAN_MAX_DUTY_PERCENT = 100;
constexpr int FAN_MAX_SPEED = 255;

int calculateDynamicFanSpeed(int temperature);
int clampFanSpeed(int speed);
