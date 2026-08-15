#include "ec_controller.h"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <utility>
#include <vector>

namespace {

int failures = 0;

struct CallResult {
  bool success;
  std::int32_t value;
};

class FakeTuxedoIoTransport : public TuxedoIoTransport {
public:
  bool available() const override {
    return deviceAvailable;
  }

  bool call(unsigned long request, std::int32_t &argument) override {
    calls.push_back(std::make_pair(request, argument));
    if (nextResult >= results.size()) {
      return false;
    }

    const CallResult RESULT = results[nextResult++];
    if (RESULT.success) {
      argument = RESULT.value;
    }
    return RESULT.success;
  }

  bool deviceAvailable = true;
  std::vector<CallResult> results;
  std::vector<std::pair<unsigned long, std::int32_t>> calls;

private:
  std::size_t nextResult = 0;
};

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

void testInitialization() {
  FakeTuxedoIoTransport unavailable;
  unavailable.deviceAvailable = false;
  EcController unavailableController(unavailable);
  expectStatus("missing device",
               EcStatus::DeviceUnavailable,
               unavailableController.initialize());
  expectTrue("missing device avoids ioctl", unavailable.calls.empty());

  FakeTuxedoIoTransport failed;
  failed.results = {{false, 0}};
  EcController failedController(failed);
  expectStatus("hardware check failure",
               EcStatus::ReadFailed,
               failedController.initialize());

  FakeTuxedoIoTransport unsupported;
  unsupported.results = {{true, 0}};
  EcController unsupportedController(unsupported);
  expectStatus("unsupported hardware",
               EcStatus::UnsupportedHardware,
               unsupportedController.initialize());

  FakeTuxedoIoTransport supported;
  supported.results = {{true, 1}};
  EcController supportedController(supported);
  expectStatus("supported hardware",
               EcStatus::Success,
               supportedController.initialize());
}

void testTemperatureRead() {
  FakeTuxedoIoTransport io;
  io.results = {{true, 1}, {true, 0x002a5507}};
  EcController ec(io);
  expectStatus("temperature init", EcStatus::Success, ec.initialize());

  std::uint8_t temperature = 0;
  expectStatus(
      "temperature read", EcStatus::Success, ec.getLocalTemp(temperature));
  expectEqual("temperature uses second byte", 42, temperature);

  FakeTuxedoIoTransport failed;
  failed.results = {{true, 1}, {false, 0}};
  EcController failedEc(failed);
  expectStatus(
      "failed temperature init", EcStatus::Success, failedEc.initialize());
  temperature = 77;
  expectStatus("failed temperature read",
               EcStatus::ReadFailed,
               failedEc.getLocalTemp(temperature));
  expectEqual("failed read preserves output", 77, temperature);
}

void testFanSpeedBoundariesAndPacking() {
  for (const int SPEED : {0, 255}) {
    FakeTuxedoIoTransport io;
    io.results = {{true, 1},
                  {true, 0x003c0064},
                  {true, 0x00000022},
                  {true, 0x00000033},
                  {true, 0}};
    EcController ec(io);
    expectStatus("fan boundary init", EcStatus::Success, ec.initialize());
    expectStatus("fan boundary write",
                 EcStatus::Success,
                 ec.setFanSpeed(static_cast<std::uint8_t>(SPEED)));
    expectEqual(
        "fan boundary call count", 5, static_cast<int>(io.calls.size()));
    expectEqual(
        "fan zero boundary packed", SPEED | 0x00332200, io.calls.back().second);
  }
}

void testFanWriteFailurePropagation() {
  FakeTuxedoIoTransport readFailure;
  readFailure.results = {{true, 1}, {true, 0x003c0064}, {false, 0}};
  EcController readFailureEc(readFailure);
  expectStatus(
      "fan read failure init", EcStatus::Success, readFailureEc.initialize());
  expectStatus(
      "fan read failure", EcStatus::ReadFailed, readFailureEc.setFanSpeed(128));
  expectEqual("fan read failure stops calls",
              3,
              static_cast<int>(readFailure.calls.size()));

  FakeTuxedoIoTransport writeFailure;
  writeFailure.results = {{true, 1},
                          {true, 0x003c0064},
                          {true, 0x00000022},
                          {true, 0x00000033},
                          {false, 0}};
  EcController writeFailureEc(writeFailure);
  expectStatus(
      "fan write failure init", EcStatus::Success, writeFailureEc.initialize());
  expectStatus("fan write failure",
               EcStatus::WriteFailed,
               writeFailureEc.setFanSpeed(128));
}

void testAutomaticMode() {
  FakeTuxedoIoTransport io;
  io.results = {{true, 1}, {true, 0}};
  EcController ec(io);
  expectStatus("auto init", EcStatus::Success, ec.initialize());
  expectStatus("auto write", EcStatus::Success, ec.setFansAuto());
  expectEqual("auto enables all fan bits", 0x0f, io.calls.back().second);
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
  testInitialization();
  testTemperatureRead();
  testFanSpeedBoundariesAndPacking();
  testFanWriteFailurePropagation();
  testAutomaticMode();
  testConsecutiveFailurePolicy();
  testTemperaturePlausibilityPolicy();
  testAcceptedTemperatureUpdatesPlausibilityBaseline();
  return failures;
}
