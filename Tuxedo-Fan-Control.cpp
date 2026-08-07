#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <sys/io.h>
#include <unistd.h>

#include "ec_controller.h"
#include "fan_curve.h"

constexpr unsigned REFRESH_RATE_MS = 250;
constexpr unsigned MAX_FAN_SET_INTERVAL_MS = 2000;

static bool ecInit() {
  if (ioperm(EC_DATA_PORT, 1, 1) != 0) {
    return false;
  }

  if (ioperm(EC_COMMAND_PORT, 1, 1) != 0) {
    ioperm(EC_DATA_PORT, 1, 0);
    return false;
  }

  return true;
}

class SystemEcPortIo : public EcPortIo {
public:
  std::uint8_t readPort(unsigned port) override {
    return inb(port);
  }

  void writePort(std::uint8_t value, unsigned port) override {
    outb(value, port);
  }
};

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
  unsigned consecutiveEcFailures = 0;
  SystemEcPortIo portIo;
  EcController ec(portIo);

  for (;;) {
    const std::uint64_t NOW = nowMilliseconds();
    std::uint8_t temperature = 0;
    EcStatus status = ec.getLocalTemp(temperature);
    if (status != EcStatus::Success) {
      std::cerr << "EC temperature read failed: " << ecStatusMessage(status)
                << '\n';
      if (ecFailureLimitReached(consecutiveEcFailures)) {
        std::cerr << "Too many consecutive EC failures; stopping fan control "
                     "without issuing an undocumented recovery command\n";
        return EXIT_FAILURE;
      }
      usleep(REFRESH_RATE_MS * 1000);
      continue;
    }

    const int TEMP = temperature;
    const int DYNAMIC_FAN_SPEED = calculateDynamicFanSpeed(TEMP, PROFILE);

    if (lastFanSpeed != DYNAMIC_FAN_SPEED ||
        NOW > lastTimeFanUpdate + MAX_FAN_SET_INTERVAL_MS) {
      status = ec.setFanSpeed(
          static_cast<std::uint8_t>(clampFanSpeed(DYNAMIC_FAN_SPEED)));
      if (status != EcStatus::Success) {
        std::cerr << "EC fan write failed: " << ecStatusMessage(status) << '\n';
        if (ecFailureLimitReached(consecutiveEcFailures)) {
          std::cerr << "Too many consecutive EC failures; stopping fan control "
                       "without issuing an undocumented recovery command\n";
          return EXIT_FAILURE;
        }
        usleep(REFRESH_RATE_MS * 1000);
        continue;
      }
      lastTimeFanUpdate = NOW;
#ifdef VERBOSE
      std::cout << "T:" << TEMP << "°C | set fan to "
                << std::round(static_cast<float>(DYNAMIC_FAN_SPEED) /
                              FAN_MAX_SPEED * 100)
                << "%\n";
#endif
    }
    resetEcFailures(consecutiveEcFailures);
    lastFanSpeed = DYNAMIC_FAN_SPEED;
    usleep(REFRESH_RATE_MS * 1000);
  }
}
