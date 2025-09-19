/*
 * Copyright 2025 LurkAndLoiter.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "nvml.h"
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef DEBUG
#define DEBUG_PRINT(fmt, ...)                                                  \
  fprintf(stderr, "DEBUG: %s:%d: " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define DEBUG_PRINT(fmt, ...)
#endif

#define TEMP_THRESHOLD 2 // Degree celcius before action
#define MIN_TEMP 55      // Lowest value from TempTargets
#define MAX_TEMP 80      // Highest value from TempTargets

unsigned int FanSpeeds[MAX_TEMP - MIN_TEMP + 1];
static unsigned int deviceCount = 0;
static volatile int terminate = 0;
static pthread_t *threads = NULL;

typedef struct {
  int id;
  unsigned int prevFanSpeed;
  unsigned int prevTemperature;
  nvmlDevice_t handle;
  unsigned int fanCount;
} Device;

void runTimeSanity(const unsigned int *TempTargets,
                   const unsigned int *FanTargets,
                   const unsigned int CountTargets) {
  // Runtime sanity checks. These are here to protect you.
  if (MIN_TEMP != TempTargets[0]) {
    DEBUG_PRINT("ERROR: MIN_TEMP does not align with TempTargets.\n");
    exit(EXIT_FAILURE);
  }
  if (MAX_TEMP != TempTargets[CountTargets - 1]) {
    DEBUG_PRINT("ERROR: MAX_TEMP does not align with TempTargets.\n");
    exit(EXIT_FAILURE);
  }
  if (TempTargets[CountTargets - 1] > 90) {
    DEBUG_PRINT("ERROR: TempTargets maximum must not exceed 90\n");
    exit(EXIT_FAILURE);
  }
  if (FanTargets[CountTargets - 1] > 100) {
    DEBUG_PRINT("ERROR: FanTargets maximum must not exceed 100\n");
    exit(EXIT_FAILURE);
  }
  for (unsigned int i = 0; i < CountTargets - 1; i++) {
    if (FanTargets[i + 1] < FanTargets[i]) {
      DEBUG_PRINT("ERROR: FanTargets must be orderd min to max\n");
      exit(EXIT_FAILURE);
    }
    if (TempTargets[i + 1] < TempTargets[i]) {
      DEBUG_PRINT("ERROR: TempTargets must be orderd min to max\n");
      exit(EXIT_FAILURE);
    }
  }
}

void cleanup(const int signum) {
  terminate = 1;
  if (threads) {
    for (unsigned int i = 0; i < deviceCount; i++) {
      pthread_join(threads[i], NULL);
    }
    free(threads);
    threads = NULL;
  }
  nvmlShutdown();
  DEBUG_PRINT("Shutdown Complete\n");
  exit(signum);
}

void signal_handler(const int signum) {
  DEBUG_PRINT("Received signal %d, shutting down...\n", signum);
  cleanup(signum);
}

static unsigned int fanspeedFromT(const unsigned int temperature,
                                  const unsigned int *slopes,
                                  const unsigned int *TempTargets,
                                  const unsigned int *FanTargets,
                                  const unsigned int CountTargets) {
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

void precalcFanSpeeds(void) {
  const unsigned int TempTargets[] = {55, 80};
  const unsigned int FanTargets[] = {40, 100};
  const unsigned int CountTargets = sizeof(FanTargets) / sizeof(FanTargets[0]);
  // Compile time sanity checks. These are here to pretect you
  _Static_assert(sizeof(TempTargets) / sizeof(TempTargets[0]) ==
                     sizeof(FanTargets) / sizeof(FanTargets[0]),
                 "TempTargets and FanTargets must have the same length");
  runTimeSanity(TempTargets, FanTargets, CountTargets);

  unsigned int slopes[CountTargets - 1];
  for (unsigned int i = 0; i < CountTargets - 1; i++) {
    slopes[i] = (FanTargets[i + 1] - FanTargets[i]) * 100 /
                (TempTargets[i + 1] - TempTargets[i]);
  }

  for (int i = MIN_TEMP; i <= MAX_TEMP; i++) {
    FanSpeeds[i - MIN_TEMP] =
        fanspeedFromT(i, slopes, TempTargets, FanTargets, CountTargets);
  }
}

unsigned int getFanSpeed(unsigned int temperature) {
  if (temperature < MIN_TEMP)
    temperature = MIN_TEMP;
  if (temperature > MAX_TEMP)
    temperature = MAX_TEMP;
  return FanSpeeds[temperature - MIN_TEMP];
}

static void nvmlStart() {
  nvmlReturn_t result;

  result = nvmlInit_v2();
  if (result != NVML_SUCCESS) {
    DEBUG_PRINT("Failed to initialize NVML: %s\n", nvmlErrorString(result));
    cleanup(EXIT_FAILURE);
  }

  result = nvmlDeviceGetCount_v2(&deviceCount);
  if (result != NVML_SUCCESS) {
    DEBUG_PRINT("Failed to get device count: %s\n", nvmlErrorString(result));
    cleanup(EXIT_FAILURE);
  } else if (deviceCount < 1) {
    DEBUG_PRINT("Unsupported: No Nvidia Devices found.\n");
    cleanup(EXIT_FAILURE);
  }
}

void *deviceLoop(void *arg) {
  Device *device = (Device *)arg;
  nvmlReturn_t result;
  unsigned int temperature;
  const unsigned int polling_interval = 1000000;

  result = nvmlDeviceGetHandleByIndex_v2(device->id, &device->handle);
  if (result != NVML_SUCCESS) {
    DEBUG_PRINT("Failed to get device %d handle: %s\n", device->id,
                nvmlErrorString(result));
    cleanup(EXIT_FAILURE);
  }

  result = nvmlDeviceGetNumFans(device->handle, &device->fanCount);
  if (result != NVML_SUCCESS) {
    DEBUG_PRINT("Failed to get fan count for device %d: %s\n", device->id,
                nvmlErrorString(result));
    cleanup(EXIT_FAILURE);
  }

  /* LOOP */
  while (!terminate) {
    result = nvmlDeviceGetTemperature(device->handle, NVML_TEMPERATURE_GPU,
                                      &temperature);
    if (result != NVML_SUCCESS) {
      DEBUG_PRINT("Failed to get temperature for device %d: %s\n", device->id,
                  nvmlErrorString(result));
      continue;
    }

    unsigned int temp_diff = device->prevTemperature > temperature
                                 ? device->prevTemperature - temperature
                                 : temperature - device->prevTemperature;
    if (temp_diff >= TEMP_THRESHOLD) {

      unsigned int fanSpeed = getFanSpeed(temperature);
      if (device->prevFanSpeed != fanSpeed) {
        for (unsigned int i = 0; i < device->fanCount; i++) {
          result = nvmlDeviceSetFanSpeed_v2(device->handle, i, fanSpeed);
          if (result != NVML_SUCCESS) {
            DEBUG_PRINT("Failed to set fan: %d to speed:%d for device:%d: %s\n",
                        i, fanSpeed, device->id, nvmlErrorString(result));
          }
        }

        device->prevTemperature = temperature;
        device->prevFanSpeed = fanSpeed;

        DEBUG_PRINT("Monitoring device: %d temp: %d->%d fans:%d@%d->%d\n",
                    device->id, device->prevTemperature, temperature,
                    device->fanCount, device->prevFanSpeed, fanSpeed);
      }
    }
    usleep((temp_diff > 5) ? polling_interval / 2 : polling_interval);
  }
  /* End LOOP */

  /* Terminate signaled reset fan control to firmware */
  for (unsigned int i = 0; i < device->fanCount; i++) {
    result = nvmlDeviceSetDefaultFanSpeed_v2(device->handle, i);
    if (result != NVML_SUCCESS) {
      DEBUG_PRINT(
          "Failed to set fan: %d to firmware default for device:%d: %s\n", i,
          device->id, nvmlErrorString(result));
    }
  }

  DEBUG_PRINT("Device %d thread terminated\n", device->id);
  free(device);
  return NULL;
}

void threadDevices() {
  threads = malloc(sizeof(pthread_t) * deviceCount);
  if (!threads) {
    DEBUG_PRINT("Failed to allocate threads array\n");
    cleanup(EXIT_FAILURE);
  }

  for (unsigned int i = 0; i < deviceCount; i++) {
    Device *device = malloc(sizeof(Device));
    if (!device) {
      DEBUG_PRINT("Failed to allocate device index\n");
      cleanup(EXIT_FAILURE);
    }

    device->id = i;
    device->prevFanSpeed = 1; // 1 avoids gate overlap with 0 RPM
    device->prevTemperature = 0;

    if (pthread_create(&threads[i], NULL, deviceLoop, device) != 0) {
      DEBUG_PRINT("Failed to create thread for device %d\n", i);
      free(device);
      cleanup(EXIT_FAILURE);
    }
  }
}

int main(void) {
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);

  precalcFanSpeeds();
  nvmlStart();
  threadDevices();

  while (!terminate) {
    pause();
  }

  cleanup(EXIT_SUCCESS);
  return EXIT_SUCCESS;
}
