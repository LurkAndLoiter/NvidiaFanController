#!/opt/fanControl/.venv/bin/python

import pynvml, time
from pynvml import *

TEMP_MIN = 55
TEMP_MAX = 80
FAN_MIN = 50
FAN_MAX = 100
TEMP_RANGE = TEMP_MAX - TEMP_MIN
FAN_RANGE = FAN_MAX - FAN_MIN
TEMP_MULTIPLIER = FAN_RANGE / TEMP_RANGE
def fanspeed_from_t(t):
  if t <= TEMP_MIN: return FAN_MIN
  if t >= TEMP_MAX: return FAN_MAX
  return (((t - TEMP_MIN) * TEMP_MULTIPLIER) + FAN_MIN)

try:
  _nvmlGetFunctionPointer = pynvml._nvmlGetFunctionPointer
  _nvmlCheckReturn = pynvml._nvmlCheckReturn
except AttributeError as err:
  _nvmlGetFunctionPointer = pynvml.nvml._nvmlGetFunctionPointer
  _nvmlCheckReturn = pynvml.nvml._nvmlCheckReturn

nvmlInit()

class Device:
  def __init__(self, index):
    self.index = index
    self.handle = nvmlDeviceGetHandleByIndex(index)
    self.name = nvmlDeviceGetName(self.handle)
    self.fan_count = nvmlDeviceGetNumFans(self.handle)
  
  def temp(self):
    return nvmlDeviceGetTemperature(self.handle, NVML_TEMPERATURE_GPU)
  
  def fan_percentages(self):
    return [nvmlDeviceGetFanSpeed_v2(self.handle, i) for i in range(self.fan_count)]

  def set_fan_speed(self, percentage):
    """ WARNING: This function changes the fan control policy to manual. It means that YOU have to monitor the temperature and adjust the fan speed accordingly. If you set the fan speed too low you can burn your GPU! Use nvmlDeviceSetDefaultFanSpeed_v2 to restore default control policy.
    """
    for i in range(self.fan_count):
      nvmlDeviceSetFanSpeed_v2(self.handle, i, percentage)

  def query(self):
    return f"{self.index}:{self.name} {self.temp()}@{self.fan_percentages()}"
  
  def control(self):
    t = self.temp()
    controlFlag = True
    fans = self.fan_percentages()
    current = round(sum(fans) / len(fans))
    shouldbe = round(fanspeed_from_t(t))
    if(shouldbe != current):
      self.set_fan_speed(shouldbe)
  
  def __str__(self):
    return f"{self.index}:{self.name} fans={self.fan_count}"
  __repr__ = __str__

device_count = nvmlDeviceGetCount()
devices = [Device(i) for i in range(device_count)]


def main():
  try:
    while True:
      for device in devices:
        device.control()
      time.sleep(1)
  finally:
    # reset to auto fan control
    for device in devices:
      for i in range(device.fan_count):
        nvmlDeviceSetDefaultFanSpeed_v2(device.handle, i)
    nvmlShutdown()

if __name__ == "__main__":
  main()
