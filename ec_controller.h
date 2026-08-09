#pragma once

#include <chrono>
#include <cstdint>
#include <functional>

constexpr unsigned EC_COMMAND_PORT = 0x66;
constexpr unsigned EC_DATA_PORT = 0x62;
constexpr std::chrono::milliseconds EC_WAIT_TIMEOUT(1000);
constexpr std::chrono::milliseconds EC_POLL_INTERVAL(1);
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
  using Clock = std::chrono::steady_clock;
  using NowFunction = std::function<Clock::time_point()>;
  using PollWaitFunction = std::function<void()>;

  explicit EcController(EcPortIo &portIo,
                        Clock::duration waitTimeout = EC_WAIT_TIMEOUT,
                        NowFunction nowFunction = Clock::now,
                        PollWaitFunction pollWaitFunction = nullptr);

  EcStatus flush();
  EcStatus sendCommand(std::uint8_t command);
  EcStatus writeData(std::uint8_t data);
  EcStatus readByte(std::uint8_t &value);
  EcStatus setFanSpeed(std::uint8_t speed);
  EcStatus getLocalTemp(std::uint8_t &temperature);

private:
  EcStatus waitForInputBuffer();

  EcPortIo &portIo;
  Clock::duration waitTimeout;
  NowFunction now;
  PollWaitFunction pollWait;
};

bool ecFailureLimitReached(unsigned &consecutiveFailures);
void resetEcFailures(unsigned &consecutiveFailures);
bool isPlausibleTemperature(std::uint8_t temperature,
                            bool hasPreviousTemperature,
                            std::uint8_t previousTemperature);
bool acceptPlausibleTemperature(std::uint8_t temperature,
                                bool &hasPreviousTemperature,
                                std::uint8_t &previousTemperature);
