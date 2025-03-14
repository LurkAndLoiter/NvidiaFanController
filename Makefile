CC=gcc

all: fanController

fanController: fanController.c
	$(CC) -o fanController fanController.c -I ./ -lnvidia-ml 

clean:
	rm fanController
