
CC      := $(CROSS_HOST)gcc
PKGCONF := $(CROSS_HOST)pkg-config
CFLAGS  := -g -Wall -Wno-unused-result -pthread -O3 $(EXTRA_CFLAGS)
LDFLAGS := -g -lm -lz -lpng16 -pthread $(EXTRA_LDFLAGS)
OBJS    := hacktv.o common.o fir.o vbidata.o teletext.o wss.o video.o mac.o dance.o videocrypt.o videocrypts.o videocrypt-ca.o syster.o syster-ca.o acp.o vits.o nicam728.o test.o ffmpeg.o file.o hackrf.o font.o subtitles.o eurocrypt.o graphics.o
PKGS    := libavcodec libavformat libavdevice libswscale libswresample libavutil libhackrf libavfilter freetype2 $(EXTRA_PKGS)

SOAPYSDR := $(shell $(PKGCONF) --exists SoapySDR && echo SoapySDR)
ifeq ($(SOAPYSDR),SoapySDR)
	OBJS += soapysdr.o
	PKGS += SoapySDR
	CFLAGS += -DHAVE_SOAPYSDR
endif

FL2K := $(shell $(PKGCONF) --exists libosmo-fl2k && echo fl2k)
ifeq ($(FL2K),fl2k)
	OBJS += fl2k.o
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

