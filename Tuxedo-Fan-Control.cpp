#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sys/io.h>
#include <unistd.h>

#include "ec_controller.h"
#include "fan_curve.h"

constexpr unsigned REFRESH_RATE_MS = 250;
constexpr unsigned MAX_FAN_SET_INTERVAL_MS = 2000;

volatile std::sig_atomic_t stopRequested = 0;

static void requestStop(int /*signal*/) {
  stopRequested = 1;
}

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

static void logProfileFallback(int argc, const char *const argv[]) {
  if (argc == 1) {
    return;
  }

  if (argc != 3 || std::strcmp(argv[1], "--profile") != 0) {
    std::cerr << "Invalid command line; using balanced fan profile\n";
    return;
  }

  if (!isKnownFanProfileName(argv[2])) {
    std::cerr << "Invalid fan profile '" << argv[2]
              << "'; using balanced fan profile\n";
  }
}

static void setFailsafeFanSpeed(EcController &ec) {
  const EcStatus STATUS = ec.setFanSpeed(FAN_MAX_SPEED);
  if (STATUS == EcStatus::Success) {
    std::clog << "Set fan to maximum speed before stopping\n";
    return;
  }

  std::cerr << "Unable to set fan to maximum speed before stopping: "
            << ecStatusMessage(STATUS) << '\n';
}

int main(int argc, const char *argv[]) {
  std::signal(SIGINT, requestStop);
  std::signal(SIGTERM, requestStop);

  logProfileFallback(argc, argv);
  const FanProfile PROFILE = fanProfileFromCommandLine(argc, argv);
  std::clog << "Using fan profile: " << fanProfileName(PROFILE) << '\n';

  if (!ecInit()) {
    std::cerr << "Unable to access the embedded controller I/O ports\n";
    return EXIT_FAILURE;
  }

  int lastFanSpeed = -1;
  std::uint64_t lastTimeFanUpdate = 0;
  unsigned consecutiveEcFailures = 0;
  bool hasPreviousTemperature = false;
  std::uint8_t previousTemperature = 0;
  SystemEcPortIo portIo;
  EcController ec(portIo);

  while (stopRequested == 0) {
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

    if (!acceptPlausibleTemperature(
            temperature, hasPreviousTemperature, previousTemperature)) {
      std::cerr << "Implausible EC temperature "
                << static_cast<int>(temperature) << "°C; skipping fan update\n";
      if (ecFailureLimitReached(consecutiveEcFailures)) {
        std::cerr << "Too many consecutive implausible EC temperatures; "
                     "stopping fan control\n";
        setFailsafeFanSpeed(ec);
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

  std::clog << "Stop requested; leaving fan at maximum speed\n";
  setFailsafeFanSpeed(ec);
  return EXIT_SUCCESS;
}
