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

class PollWaitCounter {
public:
  void wait() {
    ++waitCount;
  }

  int waitCount = 0;
};

EcController makeTimedController(FakeEcPortIo &io,
                                 FakeClock &clock,
                                 PollWaitCounter &waitCounter) {
  return EcController(
      io,
      std::chrono::microseconds(3),
      [&clock]() { return clock.now(); },
      [&waitCounter]() { waitCounter.wait(); });
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
  PollWaitCounter waitCounter;
  EcController ec = makeTimedController(io, clock, waitCounter);
  expectStatus("command input timeout",
               EcStatus::InputBufferTimeout,
               ec.sendCommand(0x99));
  expectEqual("command timeout polls until deadline",
              3,
              static_cast<int>(io.commandReadCount));
  expectEqual("command timeout waits between polls", 3, waitCounter.waitCount);
  expectTrue("command timeout prevents write", io.writes.empty());

  FakeEcPortIo writeIo;
  writeIo.commandFallback = 0x02;
  FakeClock writeClock;
  PollWaitCounter writeWaitCounter;
  EcController writeEc =
      makeTimedController(writeIo, writeClock, writeWaitCounter);
  expectStatus("data input timeout",
               EcStatus::InputBufferTimeout,
               writeEc.writeData(0x01));
  expectEqual("data timeout polls until deadline",
              3,
              static_cast<int>(writeIo.commandReadCount));
  expectEqual(
      "data timeout waits between polls", 3, writeWaitCounter.waitCount);
  expectTrue("data timeout prevents write", writeIo.writes.empty());

  FakeEcPortIo flushIo;
  flushIo.commandFallback = 0x01;
  FakeClock flushClock;
  PollWaitCounter flushWaitCounter;
  EcController flushEc =
      makeTimedController(flushIo, flushClock, flushWaitCounter);
  expectStatus("flush timeout", EcStatus::FlushTimeout, flushEc.flush());
  expectEqual("flush timeout polls until deadline",
              3,
              static_cast<int>(flushIo.commandReadCount));
  expectEqual(
      "flush timeout waits between polls", 3, flushWaitCounter.waitCount);

  FakeEcPortIo readIo;
  FakeClock readClock;
  PollWaitCounter readWaitCounter;
  EcController readEc = makeTimedController(readIo, readClock, readWaitCounter);
  std::uint8_t value = 77;
  expectStatus(
      "output timeout", EcStatus::OutputBufferTimeout, readEc.readByte(value));
  expectEqual("output timeout polls until deadline",
              3,
              static_cast<int>(readIo.commandReadCount));
  expectEqual(
      "output timeout waits between polls", 3, readWaitCounter.waitCount);
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
  PollWaitCounter fanWaitCounter;
  EcController fanEc = makeTimedController(fanIo, fanClock, fanWaitCounter);
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
  PollWaitCounter tempWaitCounter;
  EcController tempEc = makeTimedController(tempIo, tempClock, tempWaitCounter);
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

void testTemperaturePlausibilityPolicy() {
  expectTrue("normal first temperature is plausible",
             isPlausibleTemperature(42, false, 0));
  expectTrue("low impossible temperature is rejected",
             !isPlausibleTemperature(0, false, 0));
  expectTrue("high impossible temperature is rejected",
             !isPlausibleTemperature(120, false, 0));
  expectTrue("small temperature step is plausible",
             isPlausibleTemperature(62, true, 55));
  expectTrue("large temperature drop is rejected",
             !isPlausibleTemperature(20, true, 70));
  expectTrue("large temperature jump is rejected",
             !isPlausibleTemperature(100, true, 60));
}

void testAcceptedTemperatureUpdatesPlausibilityBaseline() {
  bool hasPreviousTemperature = true;
  std::uint8_t previousTemperature = 50;

  expectTrue("large but plausible step is accepted",
             acceptPlausibleTemperature(
                 80, hasPreviousTemperature, previousTemperature));
  expectEqual("accepted step refreshes baseline", 80, previousTemperature);
  expectTrue("baseline remains initialized", hasPreviousTemperature);

  expectTrue("next normal step uses refreshed baseline",
             acceptPlausibleTemperature(
                 85, hasPreviousTemperature, previousTemperature));
  expectEqual("next accepted step refreshes baseline", 85, previousTemperature);
}

} // namespace

int runEcControllerTests() {
  testBoundedWaits();
  testZeroIsValidData();
  testTransactionPropagation();
  testSuccessfulTemperatureTransaction();
  testSuccessfulFanTransaction();
  testConsecutiveFailurePolicy();
  testTemperaturePlausibilityPolicy();
  testAcceptedTemperatureUpdatesPlausibilityBaseline();
  return failures;
}
