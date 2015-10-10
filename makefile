gateway: gateway.o rfm69.o bmp085.o smbus.o xap.o
	cc -o gateway gateway.o rfm69.o bmp085.o smbus.o xap.o -lm -lwiringPi -lwiringPiDev -lcurl -lxap

gateway.o: gateway.c rfm69.h rfm69config.h nodeconfig.h
	gcc -c gateway.c

rfm69.o: rfm69.c rfm69.h rfm69config.h
	gcc -c rfm69.c

bmp085.o: bmp085.c smbus.h
	gcc -c bmp085.c

smbus.o: smbus.c smbus.h
	gcc -c smbus.c

xap.o: xap.c
	gcc -c xap.c

clean:
	rm *.o gateway
