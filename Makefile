
CC      := gcc
CFLAGS  := -g -Wall -pthread -O3
LDFLAGS := -g -lm -pthread
OBJS    := hacktv.o fir.o vbidata.o teletext.o wss.o video.o videocrypt.o syster.o nicam728.o test.o ffmpeg.o file.o hackrf.o
PKGS    := libavcodec libavformat libavdevice libswscale libswresample libavutil libhackrf

SOAPYSDR := $(shell pkg-config --exists SoapySDR && echo SoapySDR)
ifeq ($(SOAPYSDR),SoapySDR)
	OBJS += soapysdr.o
	PKGS += SoapySDR
	CFLAGS += -DHAVE_SOAPYSDR
endif

CFLAGS  += $(shell pkg-config --cflags $(PKGS))
LDFLAGS += $(shell pkg-config --libs $(PKGS))

all: hacktv

hacktv: $(OBJS)
	$(CC) -o hacktv $(OBJS) $(LDFLAGS)

%.o: %.c Makefile
	$(CC) $(CFLAGS) -c $< -o $@
	@$(CC) $(CFLAGS) -MM $< -o $(@:.o=.d)

install:
	cp -f hacktv /usr/local/bin/

clean:
	rm -f *.o *.d hacktv

-include $(OBJS:.o=.d)

