#include "nvml.h"
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifndef DEBUG
#define DEBUG 0
#endif

#define TEMP_THRESHOLD 2 // degrees Celsius
#define MAX_DEVICES 1    // Maximum number of GPUs to support

static const int TempTargets[] = {55, 80}; // Can be any number of targets
static const int FanTargets[] = {40, 100}; // Must match TempTargets length
static const int CountTargets = sizeof(FanTargets) / sizeof(FanTargets[0]);

// Compile time sanity checks. These are here to pretect you
_Static_assert(sizeof(TempTargets) / sizeof(TempTargets[0]) ==
                   sizeof(FanTargets) / sizeof(FanTargets[0]),
               "TempTargets and FanTargets must have the same length");

// Pre-calculate slopes
void initSlopes(int *slopes) {
  for (int k = 0; k < CountTargets - 1; k++) {
    slopes[k] = (FanTargets[k + 1] - FanTargets[k]) * 100 /
                (TempTargets[k + 1] - TempTargets[k]);
  }
}

unsigned int fanspeedFromT(unsigned int temperature, int *slopes) {
  if (CountTargets == 1)
    return FanTargets[0];
  if (temperature <= TempTargets[0])
    return FanTargets[0];
  if (temperature >= TempTargets[CountTargets - 1])
    return FanTargets[CountTargets - 1];

  int i;
  for (i = 0; temperature > TempTargets[i]; i++) {
    continue;
  }
  return FanTargets[i - 1] +
         ((temperature - TempTargets[i - 1]) * slopes[i - 1]) / 100;
}

nvmlReturn_t setFanSpeed(nvmlDevice_t device, unsigned int targetSpeed,
                         unsigned int fanCount) {
  nvmlReturn_t result;
  for (unsigned int j = 0; j < fanCount; j++) {
    result = nvmlDeviceSetFanSpeed_v2(device, j, targetSpeed);
    if (result != NVML_SUCCESS)
      return result;
  }
  return NVML_SUCCESS;
}

nvmlReturn_t resetFanControl(nvmlDevice_t device, unsigned int fanCount) {
  nvmlReturn_t result;
  for (unsigned int j = 0; j < fanCount; j++) {
    result = nvmlDeviceSetDefaultFanSpeed_v2(device, j);
    if (result != NVML_SUCCESS)
      return result;
  }
  return NVML_SUCCESS;
}

nvmlDevice_t devices[MAX_DEVICES];
unsigned int fanCounts[MAX_DEVICES];
unsigned int deviceCount = 0;
volatile sig_atomic_t shutdown_requested = 0;

void handleSignal(int signum) {
  shutdown_requested = 1;
  if (DEBUG) {
    printf("Signal %d received, shutdown requested.\n", signum);
  }
}

void cleanup(int signum) {
  for (unsigned int i = 0; i < deviceCount; i++) {
    if (devices[i]) {
      resetFanControl(devices[i], fanCounts[i]);
    }
  }
  nvmlShutdown();
  if (DEBUG) {
    printf("Cleanup complete, exiting with signal %d\n", signum);
  }
  exit(0);
}

int main(int argc, char *argv[]) {
  // Runtime sanity checks. These are here to protect you.
  if (TempTargets[0] < 0) {
    fprintf(stderr, "ERROR: TempTargets minimum must be at least 0\n");
    return 1;
  }
  if (TempTargets[CountTargets - 1] > 90) {
    fprintf(stderr, "ERROR: TempTargets maximum must not exceed 90\n");
    return 1;
  }
  if (FanTargets[0] < 0) {
    fprintf(stderr, "ERROR: FanTargets minimum must be at least 0\n");
    return 1;
  }
  if (FanTargets[CountTargets - 1] > 100) {
    fprintf(stderr, "ERROR: FanTargets maximum must not exceed 100\n");
    return 1;
  }
  for (int k = 0; k < CountTargets - 1; k++) {
    if (FanTargets[k + 1] < FanTargets[k]) {
      fprintf(stderr, "ERROR: FanTargets must be orderd min to max\n");
      return 1;
    }
    if (TempTargets[k + 1] < TempTargets[k]) {
      fprintf(stderr, "ERROR: TempTargets must be orderd min to max\n");
      return 1;
    }
  }

  nvmlReturn_t result;
  unsigned int temperatures[MAX_DEVICES];
  unsigned int prev_temperatures[MAX_DEVICES] = {0};
  unsigned int prev_fan_speeds[MAX_DEVICES] = {0};
  float polling_interval = 1.0;
  int slopes[CountTargets - 1];

  if (argc > 1) {
    polling_interval = atof(argv[1]);
    if (polling_interval <= 0)
      polling_interval = 1.0;
  }

  result = nvmlInit();
  if (result != NVML_SUCCESS) {
    fprintf(stderr, "Failed to initialize NVML: %s\n", nvmlErrorString(result));
    return 1;
  }

  result = nvmlDeviceGetCount(&deviceCount);
  if (result != NVML_SUCCESS) {
    fprintf(stderr, "Failed to get device count: %s\n",
            nvmlErrorString(result));
    nvmlShutdown();
    return 1;
  }

  if (deviceCount > MAX_DEVICES)
    deviceCount = MAX_DEVICES;

  struct sigaction sa;
  sa.sa_handler = handleSignal;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGQUIT, &sa, NULL);
  sigaction(SIGABRT, &sa, NULL);
  sigaction(SIGSEGV, &sa, NULL);
  sigaction(SIGILL, &sa, NULL);
  sigaction(SIGFPE, &sa, NULL);

  // Initialize devices and fan counts
  for (unsigned int i = 0; i < deviceCount; i++) {
    result = nvmlDeviceGetHandleByIndex(i, &devices[i]);
    if (result != NVML_SUCCESS) {
      fprintf(stderr, "Failed to get device %d handle: %s\n", i,
              nvmlErrorString(result));
      cleanup(0);
    }
    result = nvmlDeviceGetNumFans(devices[i], &fanCounts[i]);
    if (result != NVML_SUCCESS) {
      fprintf(stderr, "Failed to get fan count for device %d: %s\n", i,
              nvmlErrorString(result));
      cleanup(0);
    }
  }

  initSlopes(slopes);

  while (!shutdown_requested) {
    float min_sleep_time = polling_interval;
    for (unsigned int i = 0; i < deviceCount; i++) {
      result = nvmlDeviceGetTemperature(devices[i], NVML_TEMPERATURE_GPU,
                                        &temperatures[i]);
      if (result != NVML_SUCCESS) {
        fprintf(stderr, "Failed to get temperature for device %d: %s\n", i,
                nvmlErrorString(result));
        continue;
      }

      int temp_diff = abs((int)temperatures[i] - (int)prev_temperatures[i]);
      unsigned int new_fan_speed = fanspeedFromT(temperatures[i], slopes);

      if ((temp_diff >= TEMP_THRESHOLD) &&
          (new_fan_speed != prev_fan_speeds[i])) {
        result = setFanSpeed(devices[i], new_fan_speed, fanCounts[i]);
        if (result != NVML_SUCCESS) {
          fprintf(stderr, "Failed to set fan speed for device %d: %s\n", i,
                  nvmlErrorString(result));
        } else {
          prev_temperatures[i] = temperatures[i];
          prev_fan_speeds[i] = new_fan_speed;
          if (DEBUG) {
            printf("Device %d: Temp: %u°C, Fan Speed: %u%%\n", i,
                   temperatures[i], new_fan_speed);
          }
        }
      }

      float sleep_time =
          (temp_diff > 5) ? polling_interval / 2 : polling_interval;
      if (sleep_time < min_sleep_time)
        min_sleep_time = sleep_time;
    }

    usleep((useconds_t)(min_sleep_time * 1000000));
  }

  cleanup(0);
  return 0;
}
