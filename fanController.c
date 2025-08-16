#include "nvml.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifndef DEBUG
#define DEBUG 0
#endif

#define TEMP_THRESHOLD 2 // degrees Celsius
#define MAX_DEVICES 1    // Maximum number of GPUs to support

// Static temperature and fan speed targets
static const int TempTargets[] = {55, 80}; // Can be any number of targets
static const int FanTargets[] = {40, 100}; // Must match TempTargets length
static const int TARGET_COUNT = sizeof(FanTargets) / sizeof(FanTargets[0]);

int *slopes = NULL;

// Pre-calculate slopes
void initSlopes(int *slopes) {
  for (int k = 0; k < TARGET_COUNT - 1; k++) {
    slopes[k] = (FanTargets[k + 1] - FanTargets[k]) * 100 /
                (TempTargets[k + 1] - TempTargets[k]);
  }
}

unsigned int fanspeedFromT(unsigned int temperature, int *slopes) {
  if (TARGET_COUNT == 1)
    return FanTargets[0];
  if (temperature <= TempTargets[0])
    return FanTargets[0];
  if (temperature >= TempTargets[TARGET_COUNT - 1])
    return FanTargets[TARGET_COUNT - 1];

  int i;
  for (i = 0; temperature > TempTargets[i]; i++) {
    continue;
  }
  return FanTargets[i - 1] +
         ((temperature - TempTargets[i - 1]) * slopes[i - 1]) / 100;
}

nvmlReturn_t setFanSpeed(nvmlDevice_t device, unsigned int temperature,
                         unsigned int fanCount, int *slopes) {
  unsigned int targetSpeed = fanspeedFromT(temperature, slopes);
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

// Global variables
nvmlDevice_t devices[MAX_DEVICES];
unsigned int fanCounts[MAX_DEVICES];
unsigned int deviceCount = 0;

void cleanup(int signum) {
  for (unsigned int i = 0; i < deviceCount; i++) {
    if (devices[i]) {
      resetFanControl(devices[i], fanCounts[i]);
    }
  }
  if (slopes)
    free(slopes);
  slopes = NULL;
  nvmlShutdown();
  if (DEBUG) {
    printf("Cleanup complete, exiting with signal %d\n", signum);
  }
  exit(0);
}

int main(int argc, char *argv[]) {
  nvmlReturn_t result;
  unsigned int temperatures[MAX_DEVICES];
  unsigned int prev_temperatures[MAX_DEVICES] = {0};
  float polling_interval = 1.0; // Default 1 second

  // Dynamically allocate slopes array
  slopes = malloc((TARGET_COUNT - 1) * sizeof(int));
  if (!slopes) {
    if (DEBUG) {
      printf("Failed to allocate memory for slopes\n");
    }
    return 1;
  }

  // Parse command line argument for polling interval
  if (argc > 1) {
    polling_interval = atof(argv[1]);
    if (polling_interval <= 0)
      polling_interval = 1.0;
  }

  // Initialize NVML
  result = nvmlInit();
  if (result != NVML_SUCCESS) {
    if (DEBUG) {
      printf("Failed to initialize NVML: %s\n", nvmlErrorString(result));
    }
    free(slopes);
    return 1;
  }

  // Get device count and handles
  result = nvmlDeviceGetCount(&deviceCount);
  if (result != NVML_SUCCESS) {
    if (DEBUG) {
      printf("Failed to get device count: %s\n", nvmlErrorString(result));
    }
    free(slopes);
    nvmlShutdown();
    return 1;
  }

  if (deviceCount > MAX_DEVICES)
    deviceCount = MAX_DEVICES;

  // Register signal handlers
  signal(SIGINT, cleanup);
  signal(SIGTERM, cleanup);
  signal(SIGQUIT, cleanup);
  signal(SIGABRT, cleanup);
  signal(SIGSEGV, cleanup);
  signal(SIGILL, cleanup);
  signal(SIGFPE, cleanup);

  // Initialize devices and fan counts
  for (unsigned int i = 0; i < deviceCount; i++) {
    result = nvmlDeviceGetHandleByIndex(i, &devices[i]);
    if (result != NVML_SUCCESS) {
      if (DEBUG) {
        printf("Failed to get device %d handle: %s\n", i,
               nvmlErrorString(result));
      }
      cleanup(0);
    }
    result = nvmlDeviceGetNumFans(devices[i], &fanCounts[i]);
    if (result != NVML_SUCCESS) {
      if (DEBUG) {
        printf("Failed to get fan count for device %d: %s\n", i,
               nvmlErrorString(result));
      }
      cleanup(0);
    }
  }

  initSlopes(slopes); // Initialize slope calculations

  // Main monitoring loop
  while (1) {
    for (unsigned int i = 0; i < deviceCount; i++) {
      result = nvmlDeviceGetTemperature(devices[i], NVML_TEMPERATURE_GPU,
                                        &temperatures[i]);
      if (result != NVML_SUCCESS) {
        if (DEBUG) {
          printf("Failed to get temperature for device %d: %s\n", i,
                 nvmlErrorString(result));
        }
        continue;
      }

      int temp_diff = abs((int)temperatures[i] - (int)prev_temperatures[i]);
      if (temp_diff >= TEMP_THRESHOLD) {
        result = setFanSpeed(devices[i], temperatures[i], fanCounts[i], slopes);
        if (result != NVML_SUCCESS) {
          if (DEBUG) {
            printf("Failed to set fan speed for device %d: %s\n", i,
                   nvmlErrorString(result));
          }
        } else {
          prev_temperatures[i] = temperatures[i];
          if (DEBUG) {
            printf("Device %d: Temp: %u°C, Fan Speed: %u%%\n", i,
                   temperatures[i], fanspeedFromT(temperatures[i], slopes));
          }
        }
      }

      // Adaptive sleep based on temperature change
      float sleep_time =
          (temp_diff > 5) ? polling_interval / 2 : polling_interval;
      usleep((useconds_t)(sleep_time * 1000000));
    }
  }

  cleanup(0);
  return 0;
}
