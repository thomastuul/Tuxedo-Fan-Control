#include "ec_controller.h"

#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace {

// ABI constants from tuxedo-drivers 4.22.3, tuxedo_io_ioctl.h. The request
// layout is also consumed by TUXEDO Control Center 3.0.9.
constexpr unsigned IOCTL_MAGIC = 0xEC;
constexpr unsigned MAGIC_READ_CLEVO = IOCTL_MAGIC + 1;
constexpr unsigned MAGIC_WRITE_CLEVO = IOCTL_MAGIC + 2;

constexpr unsigned long R_HWCHECK_CL = _IOR(IOCTL_MAGIC, 0x05, std::int32_t *);
constexpr unsigned long R_CL_FANINFO[] = {
    _IOR(MAGIC_READ_CLEVO, 0x10, std::int32_t *),
    _IOR(MAGIC_READ_CLEVO, 0x11, std::int32_t *),
    _IOR(MAGIC_READ_CLEVO, 0x12, std::int32_t *)};
constexpr unsigned long W_CL_FANSPEED =
    _IOW(MAGIC_WRITE_CLEVO, 0x10, std::int32_t *);
constexpr unsigned long W_CL_FANAUTO =
    _IOW(MAGIC_WRITE_CLEVO, 0x11, std::int32_t *);

constexpr std::uint8_t MIN_PLAUSIBLE_TEMPERATURE = 10;
constexpr std::uint8_t MAX_PLAUSIBLE_TEMPERATURE = 110;
constexpr int MAX_PLAUSIBLE_TEMPERATURE_STEP = 30;
constexpr unsigned TEMPERATURE_SHIFT = 16;
constexpr unsigned FAN_ON_MIN_SPEED_PERCENT = 25;
constexpr unsigned CLEVO_FAN_SPEED_MAX = 0xff;

} // namespace

const char *ecStatusMessage(EcStatus status) {
  switch (status) {
  case EcStatus::Success:
    return "success";
  case EcStatus::DeviceUnavailable:
    return "tuxedo_io device is unavailable";
  case EcStatus::UnsupportedHardware:
    return "tuxedo_io did not identify a supported Clevo interface";
  case EcStatus::ReadFailed:
    return "tuxedo_io fan information read failed";
  case EcStatus::WriteFailed:
    return "tuxedo_io fan control write failed";
  case EcStatus::FanSpeedMismatch:
    return "EC did not apply the requested fan speed";
  }

  return "unknown EC error";
}

SystemTuxedoIoTransport::SystemTuxedoIoTransport(const char *devicePath)
    : fileDescriptor(open(devicePath, O_RDWR | O_CLOEXEC)) {
}

SystemTuxedoIoTransport::~SystemTuxedoIoTransport() {
  if (fileDescriptor >= 0) {
    close(fileDescriptor);
  }
}

bool SystemTuxedoIoTransport::available() const {
  return fileDescriptor >= 0;
}

bool SystemTuxedoIoTransport::call(unsigned long request,
                                   std::int32_t &argument) {
  if (!available()) {
    errno = ENODEV;
    return false;
  }
  return ioctl(fileDescriptor, request, &argument) >= 0;
}

EcController::EcController(TuxedoIoTransport &transport)
    : transport(transport) {
}

EcStatus EcController::initialize() {
  if (!transport.available()) {
    return EcStatus::DeviceUnavailable;
  }

  std::int32_t identified = 0;
  if (!transport.call(R_HWCHECK_CL, identified)) {
    return EcStatus::ReadFailed;
  }
  if (identified != 1) {
    return EcStatus::UnsupportedHardware;
  }

  initialized = true;
  return EcStatus::Success;
}

EcStatus EcController::readFanInfo(unsigned fanIndex, std::uint32_t &fanInfo) {
  if (!initialized || fanIndex >= CLEVO_FAN_COUNT) {
    return EcStatus::ReadFailed;
  }

  std::int32_t result = 0;
  if (!transport.call(R_CL_FANINFO[fanIndex], result)) {
    return EcStatus::ReadFailed;
  }
  fanInfo = static_cast<std::uint32_t>(result);
  return EcStatus::Success;
}

EcStatus EcController::getLocalTemp(std::uint8_t &temperature) {
  std::uint32_t fanInfo = 0;
  const EcStatus STATUS = readFanInfo(0, fanInfo);
  if (STATUS != EcStatus::Success) {
    return STATUS;
  }

  temperature =
      static_cast<std::uint8_t>((fanInfo >> TEMPERATURE_SHIFT) & 0xff);
  return EcStatus::Success;
}

EcStatus EcController::setFanSpeed(std::uint8_t speed,
                                   std::uint8_t *observedSpeed) {
  std::uint32_t fanInfo[CLEVO_FAN_COUNT] = {};
  for (unsigned fanIndex = 0; fanIndex < CLEVO_FAN_COUNT; ++fanIndex) {
    const EcStatus STATUS = readFanInfo(fanIndex, fanInfo[fanIndex]);
    if (STATUS != EcStatus::Success) {
      return STATUS;
    }
  }

  std::uint32_t packedSpeeds = speed;
  for (unsigned fanIndex = 1; fanIndex < CLEVO_FAN_COUNT; ++fanIndex) {
    packedSpeeds |= (fanInfo[fanIndex] & 0xff) << (fanIndex * 8);
  }

  std::int32_t argument = static_cast<std::int32_t>(packedSpeeds);
  if (!transport.call(W_CL_FANSPEED, argument)) {
    return EcStatus::WriteFailed;
  }

  std::uint32_t verificationInfo = 0;
  const EcStatus VERIFICATION_STATUS = readFanInfo(0, verificationInfo);
  if (VERIFICATION_STATUS != EcStatus::Success) {
    return VERIFICATION_STATUS;
  }

  const std::uint8_t VERIFICATION_TEMPERATURE =
      static_cast<std::uint8_t>((verificationInfo >> TEMPERATURE_SHIFT) & 0xff);
  if (!isPlausibleTemperature(VERIFICATION_TEMPERATURE, false, 0)) {
    return EcStatus::ReadFailed;
  }

  const std::uint8_t OBSERVED_SPEED =
      static_cast<std::uint8_t>(verificationInfo & 0xff);
  if (observedSpeed != nullptr) {
    *observedSpeed = OBSERVED_SPEED;
  }
  if (OBSERVED_SPEED != normalizeClevoFanSpeed(speed)) {
    return EcStatus::FanSpeedMismatch;
  }
  return EcStatus::Success;
}

EcStatus EcController::setFansAuto() {
  if (!initialized) {
    return EcStatus::WriteFailed;
  }

  std::int32_t argument = 0x0f;
  if (!transport.call(W_CL_FANAUTO, argument)) {
    return EcStatus::WriteFailed;
  }
  return EcStatus::Success;
}

bool ecFailureLimitReached(unsigned &consecutiveFailures) {
  ++consecutiveFailures;
  return consecutiveFailures >= MAX_CONSECUTIVE_EC_FAILURES;
}

void resetEcFailures(unsigned &consecutiveFailures) {
  consecutiveFailures = 0;
}

bool isPlausibleTemperature(std::uint8_t temperature,
                            bool hasPreviousTemperature,
                            std::uint8_t previousTemperature) {
  if (temperature < MIN_PLAUSIBLE_TEMPERATURE ||
      temperature > MAX_PLAUSIBLE_TEMPERATURE) {
    return false;
  }

  if (!hasPreviousTemperature) {
    return true;
  }

  return std::abs(static_cast<int>(temperature) -
                  static_cast<int>(previousTemperature)) <=
         MAX_PLAUSIBLE_TEMPERATURE_STEP;
}

bool acceptPlausibleTemperature(std::uint8_t temperature,
                                bool &hasPreviousTemperature,
                                std::uint8_t &previousTemperature) {
  if (!isPlausibleTemperature(
          temperature, hasPreviousTemperature, previousTemperature)) {
    return false;
  }

  previousTemperature = temperature;
  hasPreviousTemperature = true;
  return true;
}

std::uint8_t normalizeClevoFanSpeed(std::uint8_t speed) {
  const unsigned FAN_OFF_THRESHOLD =
      FAN_ON_MIN_SPEED_PERCENT * CLEVO_FAN_SPEED_MAX / 2 / 100;
  const unsigned FAN_ON_MIN_SPEED =
      FAN_ON_MIN_SPEED_PERCENT * CLEVO_FAN_SPEED_MAX / 100;

  if (speed < FAN_OFF_THRESHOLD) {
    return 0;
  }
  if (speed < FAN_ON_MIN_SPEED) {
    return static_cast<std::uint8_t>(FAN_ON_MIN_SPEED);
  }
  return speed;
}
