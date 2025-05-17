DEBUG ?= 0

CFLAGS = -Wall -g

ifeq ($(DEBUG), 1)
    CFLAGS += -DDEBUG
endif

CC=gcc $(CFLAGS)

all: fanController

fanController: fanController.c
	$(CC) -o fanController fanController.c -I ./ -lnvidia-ml 

clean:
	rm fanController
