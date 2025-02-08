#include <stdio.h> // For prints
#include <unistd.h> // For sleep function
#include <nvml.h>  // For NVML
#include <signal.h> // For signal handling
#include <stdlib.h> // For exit function

unsigned int fanspeedFromT(unsigned int temperature) {
    int TempTargets[] = {55,80};
    int FanTargets[] = {40,100};
    int length = sizeof(FanTargets) / sizeof(FanTargets[0]);
    int i;
    if (length == 1) return FanTargets[0]; // single target
    if (temperature <= TempTargets[0]) return FanTargets[0]; // min target
    if (temperature >= TempTargets[length-1]) return FanTargets[length-1]; // max target
    for (i = 0; temperature > TempTargets[i]; i++) {
        continue;
    } 
    return (unsigned int)(((temperature - TempTargets[i-1]) * ((FanTargets[i] - FanTargets[i-1]) / (TempTargets[i] - TempTargets[i-1]))) + FanTargets[i-1]);
}

void setFanSpeed(nvmlDevice_t device, unsigned int temperature) {
    unsigned int targetSpeed = fanspeedFromT(temperature);
    unsigned int fanCount;
    nvmlDeviceGetNumFans(device, &fanCount);

    for (unsigned int j = 0; j < fanCount; j++) {
        nvmlDeviceSetFanSpeed_v2(device, j, targetSpeed);
    }
}

// ReEnables firmware control of fanspeeds
void resetFanControl(nvmlDevice_t device) {
    unsigned int fanCount;
    nvmlDeviceGetNumFans(device, &fanCount);
    
    for (unsigned int j = 0; j < fanCount; j++) {
        nvmlDeviceSetDefaultFanSpeed_v2(device, j);
    }
}

// Global variable to hold the device handle
nvmlDevice_t device;

// Signal handler for cleanup
void cleanup(int signum) {
    if (device) {
        resetFanControl(device);
    }
    nvmlShutdown();
    exit(0);
}

int main() {
    nvmlReturn_t result;
    unsigned int deviceCount;
    unsigned int temperature;
    unsigned int previousTemperature = 0;

    // Initialize NVML
    nvmlInit();
    // Get the number of GPUs
    nvmlDeviceGetCount(&deviceCount);
    
    // Register signal handlers
    signal(SIGINT, cleanup); // Handle Ctrl+C
    signal(SIGTERM, cleanup); // Handle termination signal
    signal(SIGQUIT, cleanup);  // Handle quit signal
    signal(SIGABRT, cleanup);  // Handle abort signal
    signal(SIGSEGV, cleanup);  // Handle segmentation fault
    signal(SIGILL, cleanup);   // Handle illegal instruction
    signal(SIGFPE, cleanup);   // Handle floating-point exception
    // Loop through each GPU
    for (unsigned int i = 0; i < deviceCount; i++) {
        nvmlDeviceGetHandleByIndex(i, &device);
        
        // Monitor temperature and fan speed in a loop
        while (1) {
            nvmlDeviceGetTemperature(device, NVML_TEMPERATURE_GPU, &temperature);
            if (temperature != previousTemperature) {
                previousTemperature = temperature;
                setFanSpeed(device, temperature);
            }
            sleep(1); // Sleep for a second before checking again
        }
    }

    // Shutdown NVML
    nvmlShutdown();
    return 0;
}
