>[!warning]
>I no longer use the python version. I now use the fancontroller compiled from fancontroller.c and nvml.h. nvml headers are unmodified from nvidia and only included for the convenience of others.
>
>To compile the c use:
>
>`gcc -o MyOutputName fanController.c -I /PATH/TO/NVML.hParentFolder/ -lnvidia-ml`

# NvidiaFanController
A simple python pynvml control script for Nvidia fan control

## Dependencies
python3 with [pynvml](https://pypi.org/project/pynvml/)

## Setup
change script `#!` to point to your python executor. I personally use a [Virtual Environment](https://wiki.archlinux.org/title/Python/Virtual_environment).

## systemd service
/etc/systemd/system/SomeName.service
```bash
[Unit]
Description=Nvidia Fan Controller
After=multi.user.target

[Service]
ExecStart=/opt/fancontroller/NvidiaFanController.py
Restart=on-failure
RestartSec=1s
Type=simple

[Install]
WantedBy=multi-user.target
```
>[!important]
>make sure the ExecStart is pointing to your script location.

`sudo systemctl enable --now SomeName.service`

## Fan Curves
This is where the fan speed will be determined.
```python
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
```

- TEMP_MIN: if it's this or below use FAN_MIN for fan speed
- TEMP_MAX: if it's this or above use FAN_MAX for fan speed
- FAN_MIN, FAN_MAX: These are both integer percentage values so 50% fan speed up to 100%
>[!Caution]
>I set FAN_MIN to 50 or 50% becasue I cannot hear this. I would not recommend going below 40% most GPUs are not able to run at their pynvml reported minimum which is usally 30% and I have found from my personal experience that running at even 35% can result in sporadic behavior if you have inconsistencies in your electrical supply. For me I live by factories that often drain the supply to the point of dimming lights so I have to be extra paranoid about that stuff. But 40% is stable for me so it should be for you as well it's just there is no difference in noise level for me so I run at 50% min
