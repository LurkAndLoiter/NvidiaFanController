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
make sure the ExecStart is pointing to your script location.

`sudo systemctl enable --now SomeName.service`
