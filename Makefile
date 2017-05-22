
CC=gcc
CFLAGS=-g -Wall -pthread -O3
LDFLAGS=-g -lm -pthread

CFLAGS+=`pkg-config --cflags libavcodec libavformat libswscale libavutil`
LDFLAGS+=`pkg-config --libs libavcodec libavformat libswscale libavutil`

CFLAGS+=`pkg-config --cflags libhackrf`
LDFLAGS+=`pkg-config --libs libhackrf`

OBJS=hacktv.o video.o ffmpeg.o hackrf.o nicam728.o

all: hacktv

hacktv: $(OBJS)
	$(CC) -o hacktv $(OBJS) $(LDFLAGS)

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

install:
	cp -f hacktv /usr/local/bin/

clean:
	rm -f *.o hacktv

