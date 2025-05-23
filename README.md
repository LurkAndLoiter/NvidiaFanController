# NVIDIA GPU Fan Controller

This project is a C-based utility for controlling NVIDIA GPU fan speeds based on temperature readings using the NVIDIA Management Library (NVML). It dynamically adjusts fan speeds according to predefined temperature targets and provides robust error handling, signal management, and multi-GPU support.

## Features
- **Dynamic Fan Control**: Adjusts fan speeds based on GPU temperature using a linear interpolation between target points.
- **Multi-GPU Support**: Monitors and controls fans on multiple NVIDIA GPUs simultaneously.
- **Signal Handling**: Gracefully handles termination signals (e.g., Ctrl+C) to reset fan control to default.
- **Adaptive Polling**: Adjusts polling interval based on temperature changes for efficiency.
- **Command-Line Option**: Accepts a polling interval as an optional argument (e.g., `./fanController 0.5` for 0.5 seconds).

## Prerequisites
- NVIDIA GPU with driver support for NVML.
- NVIDIA Management Library (NVML) development files installed (typically part of the NVIDIA driver package)(included nvml.h for convenience).
- A C compiler (e.g., `gcc`).
- `make` for building with the provided Makefile.

## Building the Project
This project uses a Makefile for compilation. The `nvml.h` header is included in the repository, so you don't need to install it separately.

To build the executable `fanController` run `make` in the project folder:
```bash
make
```
### Notes for Compilation
* Ensure `libnvidia-ml.so` is available on your system (usually in `/usr/lib` or `/usr/lib64`). You may need root privileges to link against it.
* In `fanController.c` `#include "nvml.h"` assumes `nvml.h` is in the project directory, which it is in this repository.

## Usage
Run the program with an optional polling interval (in seconds):
```bash
./fanController [interval]
```
* Default interval is 1 second if not specified.
* Example: `./fanController 0.5` for a 0.5-second base polling interval.

The program will:
1. Initialize NVML and detect all NVIDIA GPUs.
2. Monitor GPU temperatures continuously.
3. Adjust fan speeds based on the temperature and predefined targets.
4. Log temperature and fan speed changes to stdout.

## Included systemd service file 
* The indcluded systemd service file will attempt to load the fanctroller binary at boot alongside items such as bluetoothd and networkd this means the fanController will already be working by the time you're at your login screen.
* nvidia-fancontroller.service assumes a file location of /opt/fanController

>[!note]
>ideally fanController will be placed in /opt/fanController placing fanController in $HOME will result in a single service fail loop but should execute on the second attempt. This is caused by systemd service file being called before $HOME is mounted.

## Configuration
### Temperature and Fan Speed Targets
The fan control logic relies on two arrays defined in `fanController.c`:
* `TempTargets`: Array of temperature thresholds (in °C).
* `FanTargets`: Array of corresponding fan speeds (as percentages, 0-100%).
#### Key Points:
* **Index Correlation:** `TempTargets` and `FanTargets` are related by their indices. For example, `TempTargets[0]` corresponds to `FanTargets[0]`.
* **Ordering:** Both arrays must be sorted from lowest to highest value. The program assumes this ordering for linear interpolation.
* **Length:** The arrays must have the same number of elements. You can have 1, 2, or more targets (e.g., `{0, 50}` or `{0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100}`).
* **Example:**
> ```c
>     static const int TempTargets[] = {55, 80};
>     static const int FanTargets[] = {40, 100};
> ```
> * At 55°C, the fan speed is 40%.
> * At 80°C or above, the fan speed is 100%.
> * Between 55°C and 80°C, the speed is linearly interpolated.

>[!warning]
>Most GPU fans have a minimum speed (often reported as 30%, but 35% might be more stable). Setting `FanTargets` below this may result in unstable behavior(fan toggling on/off).

### TEMP_THRESHOLD
* Defined as `#define TEMP_THRESHOLD 2`.
* This is the minimum temperature change (in °C) required to trigger a fan speed adjustment.
* Prevents unnecessary fan speed changes due to minor temperature fluctuations, reducing wear and noise.
* Adjust this value based on your sensitivity needs (e.g., increase to 5 for less frequent updates).

### MAX_DEVICES
* Defined as `#define MAX_DEVICES 1`.
* Limits the number of GPUs the program will manage to prevent buffer overflows.
* If you have more than 1 GPUs, increase this value and recompile.

### Code Structure
* **fanController.c:** Main source file containing all logic.
* **nvml.h:** NVIDIA Management Library header (included in the repository).
* **Makefile:** Build script for easy compilation.

### How It Works
* **Initialization:** NVML is initialized, and GPU handles are obtained.
* **Slope Calculation:** Pre-calculates slopes between temperature targets for efficient interpolation.
* **Main Loop:** Continuously monitors GPU temperatures, adjusts fan speeds if the change exceeds `TEMP_THRESHOLD`, and sleeps adaptively based on temperature variation.
* **Cleanup:** Resets fan control to firmware defaults on exit or signal interruption.

### Important Notes
* **Permissions:** Running the program may require root privileges (`sudo`) to access NVML functions.
* **Error Handling:** Check stdout for error messages if NVML calls fail (e.g., device not found, fan speed setting failed).
* **Fan Speed Stability:** Test your `FanTargets` values to ensure they work with your specific GPU model.
* **Memory:** The program dynamically allocates memory for the slopes array based on `TARGET_COUNT`. Ensure your system has sufficient memory if using many targets.
