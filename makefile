gateway: gateway.o rfm69.o
	cc -o gateway gateway.o rfm69.o -lm -lwiringPi -lwiringPiDev 

gateway.o: gateway.c rfm69.h rfm69config.h
	gcc -c gateway.c

rfm69.o: rfm69.c rfm69.h
	gcc -c rfm69.c
