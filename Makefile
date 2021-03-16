CFLAGS+=-IminIni/dev
LDFLAGS=-lm -lwiringPi -lwiringPiDev -lcurl -lmosquitto

GIT_VER:= $(shell git describe --always --dirty=+)
CFLAGS += -DGIT_VER="\"${GIT_VER}\""

gateway: gateway.o rfm69.o bmp085.o smbus.o xap.o mqtt.o minIni/dev/minIni.c opnode-libxap/src/xapcommon.c
	$(CC) -o gateway gateway.o rfm69.o bmp085.o smbus.o xap.o mqtt.o minIni/dev/minIni.c opnode-libxap/src/xapcommon.c $(LDFLAGS)

clean:
	rm *.o gateway

gateway.o: gateway.c gitversion rfm69.h rfm69config.h mqtt.h minIni/dev/minIni.h

gitversion: FORCE
	[ -f $@ ] || touch $@
	echo $(GIT_VER) | cmp -s $@ - || echo $(GIT_VER) > $@

FORCE:


