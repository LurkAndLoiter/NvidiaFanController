DEBUG ?= 0

CFLAGS = -Wall -g

ifeq ($(DEBUG), 1)
    CFLAGS += -DDEBUG
endif

CC=gcc $(CFLAGS)

all: fanController

fanController: fanController.c
	$(CC) -o fanController fanController.c -lnvidia-ml 

install: fanController
	sudo mv ./fanController /opt/fanController
	sudo cp ./nvidia-fancontroller.service /usr/lib/systemd/system/nvidia-fancontroller.service
	@if systemctl is-active --quiet nvidia-fancontroller; then \
		sudo systemctl daemon-reload; \
		echo "Reloaded nvidia-fancontroller.service"; \
	else \
		sudo systemctl enable --now nvidia-fancontroller; \
	fi

uninstall:
	@if systemctl is-active --quiet nvidia-fancontroller; then \
		sudo systemctl disable --now nvidia-fancontroller; \
	fi
	@if [ -f /opt/fanController ]; then \
		sudo rm /opt/fanController && \
		echo "Removed /opt/fanController"; \
	fi
	@if [ -f /usr/lib/systemd/system/nvidia-fancontroller.service ]; then \
		sudo rm /usr/lib/systemd/system/nvidia-fancontroller.service && \
		echo "Removed /usr/lib/systemd/system/nvidia-fancontroller.service"; \
	fi

clean:
	rm fanController
