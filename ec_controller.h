#pragma once

#include <cstdint>

constexpr unsigned EC_COMMAND_PORT = 0x66;
constexpr unsigned EC_DATA_PORT = 0x62;
constexpr unsigned EC_WAIT_ATTEMPTS = 1000000;
constexpr unsigned MAX_CONSECUTIVE_EC_FAILURES = 3;

enum class EcStatus {
  Success,
  FlushTimeout,
  InputBufferTimeout,
  OutputBufferTimeout
};

const char *ecStatusMessage(EcStatus status);

class EcPortIo {
public:
  virtual ~EcPortIo() {
  }
  virtual std::uint8_t readPort(unsigned port) = 0;
  virtual void writePort(std::uint8_t value, unsigned port) = 0;
};

class EcController {
public:
  explicit EcController(EcPortIo &portIo,
                        unsigned waitAttempts = EC_WAIT_ATTEMPTS);

  EcStatus flush();
  EcStatus sendCommand(std::uint8_t command);
  EcStatus writeData(std::uint8_t data);
  EcStatus readByte(std::uint8_t &value);
  EcStatus setFanSpeed(std::uint8_t speed);
  EcStatus getLocalTemp(std::uint8_t &temperature);

private:
  EcStatus waitForInputBuffer();

  EcPortIo &portIo;
  unsigned waitAttempts;
};

bool ecFailureLimitReached(unsigned &consecutiveFailures);
void resetEcFailures(unsigned &consecutiveFailures);
