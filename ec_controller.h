#pragma once

#include <cstdint>

constexpr const char *TUXEDO_IO_DEVICE = "/dev/tuxedo_io";
constexpr unsigned CLEVO_FAN_COUNT = 3;
constexpr unsigned FAN_SPEED_VERIFICATION_ATTEMPTS = 5;
constexpr unsigned FAN_SPEED_VERIFICATION_RETRY_MS = 100;
constexpr std::uint64_t EC_FAILURE_GRACE_PERIOD_MS = 10000;

enum class EcStatus {
  Success,
  DeviceUnavailable,
  UnsupportedHardware,
  ReadFailed,
  WriteFailed,
  FanSpeedMismatch
};

const char *ecStatusMessage(EcStatus status);

class TuxedoIoTransport {
public:
  virtual ~TuxedoIoTransport() {
  }
  virtual bool available() const = 0;
  virtual bool call(unsigned long request, std::int32_t &argument) = 0;
  virtual void waitMilliseconds(unsigned milliseconds) = 0;
};

class SystemTuxedoIoTransport : public TuxedoIoTransport {
public:
  explicit SystemTuxedoIoTransport(const char *devicePath = TUXEDO_IO_DEVICE);
  ~SystemTuxedoIoTransport() override;

  SystemTuxedoIoTransport(const SystemTuxedoIoTransport &) = delete;
  SystemTuxedoIoTransport &operator=(const SystemTuxedoIoTransport &) = delete;

  bool available() const override;
  bool call(unsigned long request, std::int32_t &argument) override;
  void waitMilliseconds(unsigned milliseconds) override;

private:
  int fileDescriptor;
};

class EcController {
public:
  explicit EcController(TuxedoIoTransport &transport);

  EcStatus initialize();
  EcStatus setFanSpeed(std::uint8_t speed,
                       std::uint8_t *observedSpeed = nullptr);
  EcStatus setFansAuto();
  EcStatus getLocalTemp(std::uint8_t &temperature);

private:
  EcStatus readFanInfo(unsigned fanIndex, std::uint32_t &fanInfo);

  TuxedoIoTransport &transport;
  bool initialized = false;
};

struct EcFailureState {
  bool active = false;
  std::uint64_t firstFailureMilliseconds = 0;
};

bool ecFailureLimitReached(EcFailureState &state,
                           std::uint64_t nowMilliseconds);
void resetEcFailures(EcFailureState &state);
bool isPlausibleTemperature(std::uint8_t temperature,
                            bool hasPreviousTemperature,
                            std::uint8_t previousTemperature);
bool acceptPlausibleTemperature(std::uint8_t temperature,
                                bool &hasPreviousTemperature,
                                std::uint8_t &previousTemperature);
std::uint8_t normalizeClevoFanSpeed(std::uint8_t speed);
