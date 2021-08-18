CFLAGS+=-Werror
LDFLAGS=-lm -lwiringPi -lwiringPiDev -lcurl -lminIni

GIT_VER:= $(shell git describe --always --dirty=+)
CFLAGS += -DGIT_VER="\"${GIT_VER}\""

gateway: gateway.o rfm69.o bmp085.o smbus.o
	$(CC) -o gateway gateway.o rfm69.o bmp085.o smbus.o $(LDFLAGS)

clean:
	rm *.o gateway

gateway.o: gateway.c gitversion rfm69.h rfm69config.h bmp085.h

gitversion: FORCE
	[ -f $@ ] || touch $@
	echo $(GIT_VER) | cmp -s $@ - || echo $(GIT_VER) > $@

FORCE:


