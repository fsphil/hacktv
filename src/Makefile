
VERSION := $(shell git log -1 --pretty='%cd' --date=format:'%Y%m%d')-$(shell git describe --dirty --always)
CC      := $(CROSS_HOST)gcc
PKGCONF := pkg-config
CFLAGS  := -g -Wall -pthread -O3 $(EXTRA_CFLAGS) -DVERSION=\"$(VERSION)\"
LDFLAGS := -g -lm -pthread $(EXTRA_LDFLAGS)
OBJS    := hacktv.o common.o fir.o vbidata.o teletext.o wss.o video.o mac.o dance.o eurocrypt.o videocrypt.o videocrypts.o syster.o acp.o vits.o vitc.o nicam728.o sis.o av.o av_test.o av_ffmpeg.o rf.o rf_file.o
PKGS    := libavcodec libavformat libavdevice libswscale libswresample libavutil $(EXTRA_PKGS)

HACKRF := $(shell $(PKGCONF) --exists libhackrf && echo hackrf)
ifeq ($(HACKRF),hackrf)
	OBJS += rf_hackrf.o
	PKGS += libhackrf
	CFLAGS += -DHAVE_HACKRF
endif

SOAPYSDR := $(shell $(PKGCONF) --exists SoapySDR && echo SoapySDR)
ifeq ($(SOAPYSDR),SoapySDR)
	OBJS += rf_soapysdr.o
	PKGS += SoapySDR
	CFLAGS += -DHAVE_SOAPYSDR
endif

FL2K := $(shell $(PKGCONF) --exists libosmo-fl2k && echo fl2k)
ifeq ($(FL2K),fl2k)
	OBJS += rf_fl2k.o
	PKGS += libosmo-fl2k
	CFLAGS += -DHAVE_FL2K
endif

CFLAGS  += $(shell $(PKGCONF) --cflags $(PKGS))
LDFLAGS += $(shell $(PKGCONF) --libs $(PKGS))

all: hacktv

hacktv: $(OBJS)
	$(CC) -o hacktv $(OBJS) $(LDFLAGS)

%.o: %.c Makefile
	$(CC) $(CFLAGS) -c $< -o $@
	@$(CC) $(CFLAGS) -MM $< -o $(@:.o=.d)

install:
	cp -f hacktv $(PREFIX)/usr/local/bin/

clean:
	rm -f *.o *.d hacktv hacktv.exe

-include $(OBJS:.o=.d)

