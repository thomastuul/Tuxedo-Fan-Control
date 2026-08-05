#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <sys/io.h>
#include <unistd.h>

#include "fan_curve.h"

constexpr unsigned EC_COMMAND_PORT = 0x66;
constexpr unsigned EC_DATA_PORT = 0x62;
constexpr unsigned TEMP_COMMAND = 0x9E;
constexpr int TEMP_INDEX = 1;
constexpr unsigned REFRESH_RATE_MS = 250;
constexpr unsigned MAX_FAN_SET_INTERVAL_MS = 2000;

static bool ecInit() {
  if (ioperm(EC_DATA_PORT, 1, 1) != 0) {
    return false;
  }

  if (ioperm(EC_COMMAND_PORT, 1, 1) != 0) {
    return false;
  }

  return true;
}

static void ecFlush() {
  while ((inb(EC_COMMAND_PORT) & 0x1) == 0x1) {
    inb(EC_DATA_PORT);
  }
}

static void sendCommand(unsigned command) {
  int tt = 0;
  while ((inb(EC_COMMAND_PORT) & 2) != 0) {
    tt++;
    if (tt > 30000) {
      break;
    }
  }

  outb(command, EC_COMMAND_PORT);
}

static void writeData(unsigned data) {
  while ((inb(EC_COMMAND_PORT) & 2) != 0) {
  }

  outb(data, EC_DATA_PORT);
}

static int readByte() {
  for (int attempts = 1000000; attempts > 0; --attempts) {
    if ((inb(EC_COMMAND_PORT) & 1) != 0) {
      return inb(EC_DATA_PORT);
    }
  }

  return 0;
}

static void setFanSpeed(int speed) {
  sendCommand(0x99);
  writeData(0x01); // ID
  writeData(speed);
}

static int getLocalTemp() {
  ecFlush();
  sendCommand(TEMP_COMMAND);
  writeData(TEMP_INDEX);
  return readByte();
}

static std::uint64_t nowMilliseconds() {
  const auto NOW = std::chrono::steady_clock::now();
  const auto MILLISECONDS =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          NOW.time_since_epoch());
  return static_cast<std::uint64_t>(MILLISECONDS.count());
}

int main(int argc, const char *argv[]) {
  const FanProfile PROFILE = fanProfileFromCommandLine(argc, argv);
  std::clog << "Using fan profile: " << fanProfileName(PROFILE) << '\n';

  if (!ecInit()) {
    std::cerr << "Unable to access the embedded controller I/O ports\n";
    return EXIT_FAILURE;
  }

  int lastFanSpeed = -1;
  std::uint64_t lastTimeFanUpdate = 0;

  for (;;) {
    const std::uint64_t NOW = nowMilliseconds();
    int temp = getLocalTemp();
    int dynamicFanSpeed = calculateDynamicFanSpeed(temp, PROFILE);

    if (lastFanSpeed != dynamicFanSpeed ||
        NOW > lastTimeFanUpdate + MAX_FAN_SET_INTERVAL_MS) {
      setFanSpeed(clampFanSpeed(dynamicFanSpeed));
      lastTimeFanUpdate = NOW;
#ifdef VERBOSE
      std::cout << "T:" << temp << "°C | set fan to "
                << std::round(static_cast<float>(dynamicFanSpeed) /
                              FAN_MAX_SPEED * 100)
                << "%\n";
#endif
    }
    lastFanSpeed = dynamicFanSpeed;
    usleep(REFRESH_RATE_MS * 1000);
  }
}
