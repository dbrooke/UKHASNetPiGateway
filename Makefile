CFLAGS+=-IminIni/dev
LDFLAGS=-lm -lwiringPi -lwiringPiDev -lcurl

gateway: gateway.o rfm69.o bmp085.o smbus.o minIni/dev/minIni.c
	$(CC) -o gateway gateway.o rfm69.o bmp085.o smbus.o minIni/dev/minIni.c $(LDFLAGS)

clean:
	rm *.o gateway
