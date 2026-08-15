#pragma once

#include <cstdint>

constexpr const char *TUXEDO_IO_DEVICE = "/dev/tuxedo_io";
constexpr unsigned MAX_CONSECUTIVE_EC_FAILURES = 3;
constexpr unsigned CLEVO_FAN_COUNT = 3;

enum class EcStatus {
  Success,
  DeviceUnavailable,
  UnsupportedHardware,
  ReadFailed,
  WriteFailed
};

const char *ecStatusMessage(EcStatus status);

class TuxedoIoTransport {
public:
  virtual ~TuxedoIoTransport() {
  }
  virtual bool available() const = 0;
  virtual bool call(unsigned long request, std::int32_t &argument) = 0;
};

class SystemTuxedoIoTransport : public TuxedoIoTransport {
public:
  explicit SystemTuxedoIoTransport(const char *devicePath = TUXEDO_IO_DEVICE);
  ~SystemTuxedoIoTransport() override;

  SystemTuxedoIoTransport(const SystemTuxedoIoTransport &) = delete;
  SystemTuxedoIoTransport &operator=(const SystemTuxedoIoTransport &) = delete;

  bool available() const override;
  bool call(unsigned long request, std::int32_t &argument) override;

private:
  int fileDescriptor;
};

class EcController {
public:
  explicit EcController(TuxedoIoTransport &transport);

  EcStatus initialize();
  EcStatus setFanSpeed(std::uint8_t speed);
  EcStatus setFansAuto();
  EcStatus getLocalTemp(std::uint8_t &temperature);

private:
  EcStatus readFanInfo(unsigned fanIndex, std::uint32_t &fanInfo);

  TuxedoIoTransport &transport;
  bool initialized = false;
};

bool ecFailureLimitReached(unsigned &consecutiveFailures);
void resetEcFailures(unsigned &consecutiveFailures);
bool isPlausibleTemperature(std::uint8_t temperature,
                            bool hasPreviousTemperature,
                            std::uint8_t previousTemperature);
bool acceptPlausibleTemperature(std::uint8_t temperature,
                                bool &hasPreviousTemperature,
                                std::uint8_t &previousTemperature);
