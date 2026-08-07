#include "ec_controller.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <utility>
#include <vector>

namespace {

int failures = 0;

class FakeEcPortIo : public EcPortIo {
public:
  std::vector<std::uint8_t> commandReads;
  std::vector<std::uint8_t> dataReads;
  std::vector<std::pair<std::uint8_t, unsigned>> writes;
  std::uint8_t commandFallback = 0;
  unsigned commandReadCount = 0;

  std::uint8_t readPort(unsigned port) override {
    if (port == EC_COMMAND_PORT) {
      ++commandReadCount;
      return next(commandReads, commandReadIndex, commandFallback);
    }
    return next(dataReads, dataReadIndex, 0);
  }

  void writePort(std::uint8_t value, unsigned port) override {
    writes.push_back(std::make_pair(value, port));
  }

private:
  static std::uint8_t next(const std::vector<std::uint8_t> &values,
                           std::size_t &index,
                           std::uint8_t fallback) {
    if (index >= values.size()) {
      return fallback;
    }
    return values[index++];
  }

  std::size_t commandReadIndex = 0;
  std::size_t dataReadIndex = 0;
};

class FakeClock {
public:
  EcController::Clock::time_point now() {
    const EcController::Clock::time_point RESULT = current;
    current += std::chrono::microseconds(1);
    return RESULT;
  }

private:
  EcController::Clock::time_point current;
};

EcController makeTimedController(FakeEcPortIo &io, FakeClock &clock) {
  return EcController(
      io, std::chrono::microseconds(3), [&clock]() { return clock.now(); });
}

void expectTrue(const char *name, bool condition) {
  if (condition) {
    return;
  }
  std::cerr << name << ": expected true\n";
  ++failures;
}

void expectEqual(const char *name, int expected, int actual) {
  if (expected == actual) {
    return;
  }
  std::cerr << name << ": expected " << expected << ", got " << actual << '\n';
  ++failures;
}

void expectStatus(const char *name, EcStatus expected, EcStatus actual) {
  expectEqual(name, static_cast<int>(expected), static_cast<int>(actual));
}

void testBoundedWaits() {
  FakeEcPortIo io;
  io.commandFallback = 0x02;
  FakeClock clock;
  EcController ec = makeTimedController(io, clock);
  expectStatus("command input timeout",
               EcStatus::InputBufferTimeout,
               ec.sendCommand(0x99));
  expectEqual("command timeout polls until deadline",
              3,
              static_cast<int>(io.commandReadCount));
  expectTrue("command timeout prevents write", io.writes.empty());

  FakeEcPortIo writeIo;
  writeIo.commandFallback = 0x02;
  FakeClock writeClock;
  EcController writeEc = makeTimedController(writeIo, writeClock);
  expectStatus("data input timeout",
               EcStatus::InputBufferTimeout,
               writeEc.writeData(0x01));
  expectEqual("data timeout polls until deadline",
              3,
              static_cast<int>(writeIo.commandReadCount));
  expectTrue("data timeout prevents write", writeIo.writes.empty());

  FakeEcPortIo flushIo;
  flushIo.commandFallback = 0x01;
  FakeClock flushClock;
  EcController flushEc = makeTimedController(flushIo, flushClock);
  expectStatus("flush timeout", EcStatus::FlushTimeout, flushEc.flush());
  expectEqual("flush timeout polls until deadline",
              3,
              static_cast<int>(flushIo.commandReadCount));

  FakeEcPortIo readIo;
  FakeClock readClock;
  EcController readEc = makeTimedController(readIo, readClock);
  std::uint8_t value = 77;
  expectStatus(
      "output timeout", EcStatus::OutputBufferTimeout, readEc.readByte(value));
  expectEqual("output timeout polls until deadline",
              3,
              static_cast<int>(readIo.commandReadCount));
  expectEqual("timeout leaves output unchanged", 77, value);
}

void testZeroIsValidData() {
  FakeEcPortIo io;
  io.commandReads = {0x01};
  io.dataReads = {0x00};
  EcController ec(io);
  std::uint8_t value = 99;
  expectStatus("zero read status", EcStatus::Success, ec.readByte(value));
  expectEqual("zero read value", 0, value);
}

void testTransactionPropagation() {
  FakeEcPortIo fanIo;
  fanIo.commandReads = {0x00};
  fanIo.commandFallback = 0x02;
  FakeClock fanClock;
  EcController fanEc = makeTimedController(fanIo, fanClock);
  expectStatus("fan transaction propagates timeout",
               EcStatus::InputBufferTimeout,
               fanEc.setFanSpeed(128));
  expectEqual("fan transaction stops after command",
              1,
              static_cast<int>(fanIo.writes.size()));
  expectEqual("fan command value", 0x99, fanIo.writes[0].first);
  expectEqual("fan command port",
              static_cast<int>(EC_COMMAND_PORT),
              static_cast<int>(fanIo.writes[0].second));

  FakeEcPortIo tempIo;
  tempIo.commandReads = {0x00, 0x00, 0x00};
  FakeClock tempClock;
  EcController tempEc = makeTimedController(tempIo, tempClock);
  std::uint8_t temperature = 88;
  expectStatus("temperature transaction propagates read timeout",
               EcStatus::OutputBufferTimeout,
               tempEc.getLocalTemp(temperature));
  expectEqual("temperature transaction issued two writes",
              2,
              static_cast<int>(tempIo.writes.size()));
  expectEqual("failed temperature remains unchanged", 88, temperature);
}

void testSuccessfulTemperatureTransaction() {
  FakeEcPortIo io;
  io.commandReads = {0x00, 0x00, 0x00, 0x01};
  io.dataReads = {42};
  EcController ec(io);
  std::uint8_t temperature = 0;
  expectStatus(
      "temperature success", EcStatus::Success, ec.getLocalTemp(temperature));
  expectEqual("temperature value", 42, temperature);
  expectEqual("temperature write count", 2, static_cast<int>(io.writes.size()));
  expectEqual("temperature command", 0x9E, io.writes[0].first);
  expectEqual("temperature command port",
              static_cast<int>(EC_COMMAND_PORT),
              static_cast<int>(io.writes[0].second));
  expectEqual("temperature index", 0x01, io.writes[1].first);
  expectEqual("temperature data port",
              static_cast<int>(EC_DATA_PORT),
              static_cast<int>(io.writes[1].second));
}

void testSuccessfulFanTransaction() {
  FakeEcPortIo io;
  io.commandReads = {0x00, 0x00, 0x00};
  EcController ec(io);
  expectStatus("fan success", EcStatus::Success, ec.setFanSpeed(128));
  expectEqual("fan write count", 3, static_cast<int>(io.writes.size()));
  expectEqual("fan command", 0x99, io.writes[0].first);
  expectEqual("fan id", 0x01, io.writes[1].first);
  expectEqual("fan speed", 128, io.writes[2].first);
}

void testConsecutiveFailurePolicy() {
  unsigned consecutiveFailures = 0;
  expectTrue("first failure continues",
             !ecFailureLimitReached(consecutiveFailures));
  expectTrue("second failure continues",
             !ecFailureLimitReached(consecutiveFailures));
  expectTrue("third failure stops", ecFailureLimitReached(consecutiveFailures));
  resetEcFailures(consecutiveFailures);
  expectEqual(
      "success resets failures", 0, static_cast<int>(consecutiveFailures));
}

} // namespace

int runEcControllerTests() {
  testBoundedWaits();
  testZeroIsValidData();
  testTransactionPropagation();
  testSuccessfulTemperatureTransaction();
  testSuccessfulFanTransaction();
  testConsecutiveFailurePolicy();
  return failures;
}
