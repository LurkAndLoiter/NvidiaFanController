fanController: fanController.c
	$(CC) -o fanController fanController.c -I ./ -lnvidia-ml 
