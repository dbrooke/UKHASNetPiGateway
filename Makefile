CFLAGS+=-IminIni/dev
LDFLAGS=-lm -lwiringPi -lwiringPiDev -lcurl -lxap

gateway: gateway.o rfm69.o bmp085.o smbus.o xap.o minIni/dev/minIni.c
	$(CC) -o gateway gateway.o rfm69.o bmp085.o smbus.o xap.o minIni/dev/minIni.c $(LDFLAGS)

clean:
	rm *.o gateway
