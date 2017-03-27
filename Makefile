CFLAGS+=-IminIni/dev
LDFLAGS=-lm -lwiringPi -lwiringPiDev -lcurl

gateway: gateway.o rfm69.o bmp085.o smbus.o xap.o minIni/dev/minIni.c opnode-libxap/src/xapcommon.c
	$(CC) -o gateway gateway.o rfm69.o bmp085.o smbus.o xap.o minIni/dev/minIni.c opnode-libxap/src/xapcommon.c $(LDFLAGS)

clean:
	rm *.o gateway
