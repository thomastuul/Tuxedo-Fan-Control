#include <chrono>
#include <cstdlib>
#include <iostream>
#include <math.h>
#include <sys/io.h>
#include <unistd.h>

#include "fan_curve.h"

using namespace std;

#define EC_COMMAND_PORT 0x66
#define EC_DATA_PORT 0x62
#define TEMP 0x9E

#define REFRESH_RATE 250 // time to wait between each controller loop (ms)
#define MAX_FAN_SET_INTERVAL                                                   \
  2000 // maximal time between two fan rate send command

static int ecInit() {
  if (ioperm(EC_DATA_PORT, 1, 1) != 0) {
    return 1;
  }

  if (ioperm(EC_COMMAND_PORT, 1, 1) != 0) {
    return 1;
  }

  return 0;
}

static void ecFlush() {
  while ((inb(EC_COMMAND_PORT) & 0x1) == 0x1) {
    inb(EC_DATA_PORT);
  }
}

static void sendCommand(int command) {
  int tt = 0;
  while ((inb(EC_COMMAND_PORT) & 2)) {
    tt++;
    if (tt > 30000) {
      break;
    }
  }

  outb(command, EC_COMMAND_PORT);
}

static void writeData(int data) {
  while ((inb(EC_COMMAND_PORT) & 2))
    ;

  outb(data, EC_DATA_PORT);
}

static int readByte() {
  int i = 1000000;
  while ((inb(EC_COMMAND_PORT) & 1) == 0 && i > 0) {
    i -= 1;
  }

  if (i == 0) {
    return 0;
  } else {
    return inb(EC_DATA_PORT);
  }
}

static void setFanSpeed(int speed) {
  ecInit();
  sendCommand(0x99);
  writeData(0x01); // ID
  writeData(speed);
}

static int getLocalTemp() {
  int index = 1;
  ecInit();
  ecFlush();
  sendCommand(TEMP);
  writeData(index);
  // readByte();
  int value = readByte();
  return value;
}

static unsigned int time() {
  chrono::milliseconds ms = chrono::duration_cast<chrono::milliseconds>(
      chrono::system_clock::now().time_since_epoch());
  unsigned int time = ms.count();
  return time;
}

int main(int argc, char *argv[]) {
  int lastFanSpeed =
      -1; // last fan speed value, used to avoid write speed if not necessary
  int slidingMaxFanSpeed =
      -1; // last max speed value, used in combination with FAN_PEAK_HOLD_TIME
  unsigned int maxFanSpeedTime =
      0; // time at which the last max was reached, used in combination with
         // FAN_PEAK_HOLD_TIME
  unsigned int lastTimeFanUpdate =
      0; // use this to periodically set the temp unconditionnaly (useful when
         // wake of from sleep)
  while (1) {
    int temp = getLocalTemp();
    // dynamic fan speed is the computed instantaneous speed, whithout
    // hysteresis (FAN_PEAK_HOLD_TIME)
    int dynamicFanSpeed = calculateDynamicFanSpeed(temp);

    if (dynamicFanSpeed > slidingMaxFanSpeed ||
        time() > maxFanSpeedTime +
                     FAN_PEAK_HOLD_TIME) { // update max values if max is
                                           // overcome or if the time
                                           // (FAN_PEAK_HOLD_TIME) is reached
      slidingMaxFanSpeed = dynamicFanSpeed;
      maxFanSpeedTime = time();
    }
    if (lastFanSpeed != slidingMaxFanSpeed ||
        lastTimeFanUpdate + MAX_FAN_SET_INTERVAL <
            time()) { // send value if it changed or if we didn't do it since
                      // more than "MAX_FAN_SET_INTERVAL" seconds.
      setFanSpeed(clampFanSpeed(slidingMaxFanSpeed));
      lastTimeFanUpdate = time();
#ifdef VERBOSE
      cout << "T:" << temp << "°C | set fan to "
           << round((float)(slidingMaxFanSpeed) / 255 * 100) << "%";
#endif
    }
    cout << '\n';
    lastFanSpeed = slidingMaxFanSpeed;
    usleep(REFRESH_RATE * 1000);
  }
  return 0;
}
