# NVIDIA GPU Fan Controller

This project is a C-based utility for controlling NVIDIA GPU fan speeds based on temperature readings using the NVIDIA Management Library (NVML). It dynamically adjusts fan speeds according to predefined temperature targets and provides robust error handling, signal management, and multi-GPU support.

## Features

- **Dynamic Fan Control**: Adjusts fan speeds based on GPU temperature using a linear interpolation between target points.
- **Multi-GPU Support**: Monitors and controls fans on multiple NVIDIA GPUs simultaneously.
- **Signal Handling**: Gracefully handles termination signals (e.g., Ctrl+C) to reset fan control to default.
- **Adaptive Polling**: Adjusts polling interval based on temperature changes for efficiency.

## Prerequisites

- NVIDIA GPU with driver support for NVML.
- NVIDIA Management Library (NVML) development files installed (typically part of the NVIDIA driver package)(included nvml.h for convenience).
- A C compiler (e.g., `gcc`).
- `make` for building with the provided Makefile.

## Building the Project

Build options:

1. **Build with Debug Output** *For development with detailed stdout logging (e.g., `Device 0: Temp: 40°C, Fan Speed: 40%`):*
```bash
make DEBUG=1
```
2. **Build Production Executable** *Creates the `fanController` binary for manual use:*
```bash
make
```
3. **Install to System** *Installs or updates the `fanController` binary to your system (requires root privileges):*
```bash
sudo make install
```
4. **Uninstall from System** *Removes the `fanController` binary from your system (requires root privileges):*
```bash
sudo make uninstall
```

### Notes for Compilation

- Ensure `libnvidia-ml.so` is available on your system (usually in `/usr/lib` or `/usr/lib64`). You may need root privileges to link against it.

## Usage

The program will:

1. Precalculate temperature to an array of targets for reduced runtime overhead.
2. Initialize NVML and detect all NVIDIA GPUs.
3. Create a thread for all NVIDIA GPUs.
4. Monitor GPU temperature continuously.
5. Adjust fan speeds based on the temperature and predefined targets.
6. On termination cleanup and reset fans to firmware control.

## Systemd service file

- The included systemd service file will attempt to load the fanController binary at boot. fanController will already be working by the time you're at your login screen.
- nvidia-fancontroller.service assumes a fanController location of /opt/fanController adjust according to your setup.

> [!note]
> ideally fanController will be placed in /opt/fanController placing fanController in $HOME will result in a single service fail loop but should execute on the second attempt. This is caused by systemd service file being called before $HOME is mounted.

## Configuration

### Temperature and Fan Speed Targets

The fan control logic relies on two arrays defined in `fanController.c`:

- `TempTargets`: Array of temperature thresholds (in °C).
- `FanTargets`: Array of corresponding fan speeds (as percentages, 0-100%).

#### Key Points

- **Index Correlation:** `TempTargets` and `FanTargets` are related by their indices. For example, `TempTargets[0]` corresponds to `FanTargets[0]`.
- **Ordering:** Both arrays must be sorted from lowest to highest value. The program assumes this ordering for linear interpolation.
- **Length:** The arrays must have the same number of elements. You can have 1, 2, or more targets (e.g., `{0, 50}` or `{0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100}`).
- **Example:**
  > ```c
  >     static const int TempTargets[] = {55, 80};
  >     static const int FanTargets[] = {40, 100};
  > ```
  >
  > - At 55°C or below, the fan speed is 40%.
  > - At 80°C or above, the fan speed is 100%.
  > - Between 55°C and 80°C, the speed is linearly interpolated.

- **Example with 0 RPM:**
  > ```c
  >     static const int TempTargets[] = {54, 55, 80};
  >     static const int FanTargets[] = {0, 40, 100};
  > ```
  > - At 54°C or below, the fan speed is 0%
  > - At 55°C, the fan speed is 40%.
  > - At 80°C or above, the fan speed is 100%.
  > - Between 55°C and 80°C, the speed is linearly interpolated.

> [!warning]
> Most GPU fans have a minimum speed (often reported as 30%, but 35% might be more stable). Setting `FanTargets` below this may result in unstable behavior(fan toggling on/off).

### TEMP_THRESHOLD

- Defined as `#define TEMP_THRESHOLD 2`.
- This is the minimum temperature change (in °C) required to trigger a fan speed adjustment.
- Prevents unnecessary fan speed changes due to minor temperature fluctuations, reducing wear and noise.
- Adjust this value based on your sensitivity needs (e.g., increase to 5 for less frequent updates).


### Code Structure

- **fanController.c:** Main source file containing all logic.
- **nvml.h:** NVIDIA Management Library header (included in the repository).
- **Makefile:** Build script for easy compilation.

### How It Works

- **Speed Calculation:** Pre-calculates fan speeds to temperature targets for efficient interpolation.
- **Initialization:** NVML is initialized, and GPU handles are obtained.
- **Threading:** Threads are made for every device. 
- **Device Loop:**
  - Continuously monitors GPU temperatures
  - Adjusts fan speeds if the change exceeds `TEMP_THRESHOLD` and `fanSpeed` is a new speed
  - Sleeps adaptively based on temperature variation.
  - On termination resets fan control to firmware defauls
- **Cleanup:** Allows threads to close gracefully.

### Important Notes

- **Permissions:** Running the program directly may require root privileges (`sudo`) to access NVML functions.
- **Fan Speed Stability:** Test your `FanTargets` values to ensure they work with your specific GPU model.
