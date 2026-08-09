#include "ec_controller.h"

#include <cstdlib>
#include <thread>
#include <utility>

namespace {

constexpr std::uint8_t OUTPUT_BUFFER_FULL = 0x01;
constexpr std::uint8_t INPUT_BUFFER_FULL = 0x02;
constexpr std::uint8_t FAN_COMMAND = 0x99;
constexpr std::uint8_t FAN_ID = 0x01;
constexpr std::uint8_t TEMP_COMMAND = 0x9E;
constexpr std::uint8_t TEMP_INDEX = 0x01;
constexpr std::uint8_t MIN_PLAUSIBLE_TEMPERATURE = 10;
constexpr std::uint8_t MAX_PLAUSIBLE_TEMPERATURE = 110;
constexpr int MAX_PLAUSIBLE_TEMPERATURE_STEP = 30;

void defaultPollWait() {
  std::this_thread::sleep_for(EC_POLL_INTERVAL);
}

} // namespace

const char *ecStatusMessage(EcStatus status) {
  switch (status) {
  case EcStatus::Success:
    return "success";
  case EcStatus::FlushTimeout:
    return "timed out while flushing EC output";
  case EcStatus::InputBufferTimeout:
    return "timed out waiting for EC input buffer";
  case EcStatus::OutputBufferTimeout:
    return "timed out waiting for EC output data";
  }

  return "unknown EC error";
}

EcController::EcController(EcPortIo &portIo,
                           Clock::duration waitTimeout,
                           NowFunction nowFunction,
                           PollWaitFunction pollWaitFunction)
    : portIo(portIo), waitTimeout(waitTimeout), now(std::move(nowFunction)),
      pollWait(std::move(pollWaitFunction)) {
  if (!pollWait) {
    pollWait = defaultPollWait;
  }
}

EcStatus EcController::flush() {
  const Clock::time_point DEADLINE = now() + waitTimeout;
  do {
    if ((portIo.readPort(EC_COMMAND_PORT) & OUTPUT_BUFFER_FULL) == 0) {
      return EcStatus::Success;
    }
    portIo.readPort(EC_DATA_PORT);
    pollWait();
  } while (now() < DEADLINE);

  return EcStatus::FlushTimeout;
}

EcStatus EcController::waitForInputBuffer() {
  const Clock::time_point DEADLINE = now() + waitTimeout;
  do {
    if ((portIo.readPort(EC_COMMAND_PORT) & INPUT_BUFFER_FULL) == 0) {
      return EcStatus::Success;
    }
    pollWait();
  } while (now() < DEADLINE);

  return EcStatus::InputBufferTimeout;
}

EcStatus EcController::sendCommand(std::uint8_t command) {
  const EcStatus STATUS = waitForInputBuffer();
  if (STATUS != EcStatus::Success) {
    return STATUS;
  }

  portIo.writePort(command, EC_COMMAND_PORT);
  return EcStatus::Success;
}

EcStatus EcController::writeData(std::uint8_t data) {
  const EcStatus STATUS = waitForInputBuffer();
  if (STATUS != EcStatus::Success) {
    return STATUS;
  }

  portIo.writePort(data, EC_DATA_PORT);
  return EcStatus::Success;
}

EcStatus EcController::readByte(std::uint8_t &value) {
  const Clock::time_point DEADLINE = now() + waitTimeout;
  do {
    if ((portIo.readPort(EC_COMMAND_PORT) & OUTPUT_BUFFER_FULL) != 0) {
      value = portIo.readPort(EC_DATA_PORT);
      return EcStatus::Success;
    }
    pollWait();
  } while (now() < DEADLINE);

  return EcStatus::OutputBufferTimeout;
}

EcStatus EcController::setFanSpeed(std::uint8_t speed) {
  EcStatus status = sendCommand(FAN_COMMAND);
  if (status != EcStatus::Success) {
    return status;
  }
  status = writeData(FAN_ID);
  if (status != EcStatus::Success) {
    return status;
  }
  return writeData(speed);
}

EcStatus EcController::getLocalTemp(std::uint8_t &temperature) {
  EcStatus status = flush();
  if (status != EcStatus::Success) {
    return status;
  }
  status = sendCommand(TEMP_COMMAND);
  if (status != EcStatus::Success) {
    return status;
  }
  status = writeData(TEMP_INDEX);
  if (status != EcStatus::Success) {
    return status;
  }
  return readByte(temperature);
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
